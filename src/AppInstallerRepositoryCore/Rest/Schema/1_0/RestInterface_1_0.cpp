// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"
#include "Rest/Schema/1_0/Interface.h"
#include "Rest/Schema/IRestClient.h"
#include "Rest/Schema/HttpClientHelper.h"
#include "Rest/Schema/JsonHelper.h"
#include "winget/ManifestValidation.h"
#include "Rest/Schema/RestHelper.h"
#include "Rest/Schema/CommonRestConstants.h"
#include "Rest/Schema/1_0/Json/ManifestDeserializer.h"
#include "Rest/Schema/1_0/Json/SearchResponseDeserializer.h"
#include "Rest/Schema/1_0/Json/SearchRequestSerializer.h"

using namespace std::string_view_literals;
using namespace AppInstaller::Repository::Rest::Schema::V1_0::Json;

namespace AppInstaller::Repository::Rest::Schema::V1_0
{
    namespace
    {
        web::json::value GetSearchBody(const SearchRequest& searchRequest)
        {
            SearchRequestSerializer serializer;
            return serializer.Serialize(searchRequest);
        }

        utility::string_t GetSearchEndpoint(const std::string& restApiUri)
        {
            return RestHelper::AppendPathToUri(JsonHelper::GetUtilityString(restApiUri), JsonHelper::GetUtilityString(ManifestSearchPostEndpoint));
        }

        utility::string_t GetManifestByVersionEndpoint(
            const std::string& restApiUri, const std::string& packageId, const std::map<std::string_view, std::string>& queryParameters)
        {
            utility::string_t getManifestEndpoint = RestHelper::AppendPathToUri(
                JsonHelper::GetUtilityString(restApiUri), JsonHelper::GetUtilityString(ManifestByVersionAndChannelGetEndpoint));

            utility::string_t getManifestWithPackageIdPath = RestHelper::AppendPathToUri(getManifestEndpoint, JsonHelper::GetUtilityString(packageId));

            // Create the endpoint with query parameters
            return RestHelper::AppendQueryParamsToUri(getManifestWithPackageIdPath, queryParameters);
        }
    }

    Interface::Interface(const std::string& restApi, const HttpClientHelper& httpClientHelper) : m_restApiUri(restApi), m_httpClientHelper(httpClientHelper)
    {
        THROW_HR_IF(APPINSTALLER_CLI_ERROR_RESTSOURCE_INVALID_URL, !RestHelper::IsValidUri(JsonHelper::GetUtilityString(restApi)));

        m_searchEndpoint = GetSearchEndpoint(m_restApiUri);
        m_requiredRestApiHeaders.emplace(JsonHelper::GetUtilityString(ContractVersion), JsonHelper::GetUtilityString(GetVersion().ToString()));
    }

    Utility::Version Interface::GetVersion() const
    {
        return Version_1_0_0;
    }

    IRestClient::SearchResult Interface::Search(const SearchRequest& request) const
    {
        // Optimization
        if (MeetsOptimizedSearchCriteria(request))
        {
            return OptimizedSearch(request);
        }

        return SearchInternal(request);
    }

    IRestClient::SearchResult Interface::SearchInternal(const SearchRequest& request) const
    {
        SearchResult results;
        utility::string_t continuationToken;
        std::unordered_map<utility::string_t, utility::string_t> searchHeaders = m_requiredRestApiHeaders;
        do
        {
            if (!continuationToken.empty())
            {
                AICLI_LOG(Repo, Verbose, << "Received continuation token. Retrieving more results.");
                searchHeaders.insert_or_assign(JsonHelper::GetUtilityString(ContinuationToken), continuationToken);
            }

            std::optional<web::json::value> jsonObject = m_httpClientHelper.HandlePost(m_searchEndpoint, GetSearchBody(request), searchHeaders);

            utility::string_t ct;
            if (jsonObject)
            {
                SearchResponseDeserializer searchResponseDeserializer;
                SearchResult currentResult = searchResponseDeserializer.Deserialize(jsonObject.value());
                
                size_t insertElements = !request.MaximumResults ? currentResult.Matches.size() :
                    std::min(currentResult.Matches.size(), request.MaximumResults - results.Matches.size());

                std::move(currentResult.Matches.begin(), std::next(currentResult.Matches.begin(), insertElements), std::inserter(results.Matches, results.Matches.end()));
                ct = RestHelper::GetContinuationToken(jsonObject.value()).value_or(L"");
            }

            continuationToken = ct;

        } while (!continuationToken.empty() && (!request.MaximumResults || results.Matches.size() < request.MaximumResults));

        if (results.Matches.empty())
        {
            AICLI_LOG(Repo, Verbose, << "No search results returned by rest source");
        }

        return results;
    }

    std::optional<Manifest::Manifest> Interface::GetManifestByVersion(const std::string& packageId, const std::string& version, const std::string& channel) const
    {
        std::map<std::string_view, std::string> queryParams;
        if (!version.empty())
        {
            queryParams.emplace(VersionQueryParam, version);
        }

        if (!channel.empty())
        {
            queryParams.emplace(ChannelQueryParam, channel);
        }

        std::vector<Manifest::Manifest> manifests = GetManifests(packageId, queryParams);

        if (!manifests.empty())
        {
            for (Manifest::Manifest manifest : manifests)
            {
                if (Utility::CaseInsensitiveEquals(manifest.Version, version) &&
                    Utility::CaseInsensitiveEquals(manifest.Channel, channel))
                {
                    return manifest;
                }
            }
        }

        return {};
    }

    bool Interface::MeetsOptimizedSearchCriteria(const SearchRequest& request) const
    {
        // Optimization: If the user wants to install a certain package with an exact match on package id and a particular rest source, we will
        // call the package manifest endpoint to get the manifest directly instead of running a search for it.
        if (!request.Query && request.Inclusions.size() == 0 &&
            request.Filters.size() == 1 && request.Filters[0].Field == PackageMatchField::Id &&
            request.Filters[0].Type == MatchType::Exact)
        {
            AICLI_LOG(Repo, Verbose, << "Search request meets optimized search criteria.");
            return true;
        }

        return false;
    }

    IRestClient::SearchResult Interface::OptimizedSearch(const SearchRequest& request) const
    {
        SearchResult searchResult;
        std::vector<Manifest::Manifest> manifests = GetManifests(request.Filters[0].Value);

        if (!manifests.empty())
        {
            auto& manifest = manifests.at(0);
            PackageInfo packageInfo = PackageInfo{
                manifest.Id,
                manifest.DefaultLocalization.Get<AppInstaller::Manifest::Localization::PackageName>(),
                manifest.DefaultLocalization.Get<AppInstaller::Manifest::Localization::Publisher>() };

            // Add all the versions to the package info object
            std::vector<VersionInfo> versions;
            for (auto& manifestVersion : manifests)
            {
                std::vector<std::string> packageFamilyNames;
                std::vector<std::string> productCodes;

                for (auto& installer : manifestVersion.Installers)
                {
                    if (!installer.PackageFamilyName.empty())
                    {
                        packageFamilyNames.emplace_back(installer.PackageFamilyName);
                    }

                    if (!installer.ProductCode.empty())
                    {
                        productCodes.emplace_back(installer.ProductCode);
                    }
                }

                std::vector<std::string> uniquePackageFamilyNames = RestHelper::GetUniqueItems(packageFamilyNames);
                std::vector<std::string> uniqueProductCodes = RestHelper::GetUniqueItems(productCodes);

                versions.emplace_back(
                    VersionInfo{ AppInstaller::Utility::VersionAndChannel {manifestVersion.Version, manifestVersion.Channel},
                    manifestVersion, std::move(uniquePackageFamilyNames), std::move(uniqueProductCodes) });
            }

            Package package = Package{ std::move(packageInfo), std::move(versions) };
            searchResult.Matches.emplace_back(std::move(package));
        }

        return searchResult;
    }

    std::vector<Manifest::Manifest> Interface::GetManifests(const std::string& packageId, const std::map<std::string_view, std::string>& params) const
    {
        std::vector<Manifest::Manifest> results;
        std::optional<web::json::value> jsonObject = m_httpClientHelper.HandleGet(GetManifestByVersionEndpoint(m_restApiUri, packageId, params), m_requiredRestApiHeaders);

        if (!jsonObject)
        {
            AICLI_LOG(Repo, Verbose, << "No results were returned by the rest source for package id: " << packageId);
            return results;
        }

        // Parse json and return Manifests
        ManifestDeserializer manifestDeserializer;
        std::vector<Manifest::Manifest> manifests = manifestDeserializer.Deserialize(jsonObject.value());

        // Manifest validation
        for (auto& manifestItem : manifests)
        {
            std::vector<AppInstaller::Manifest::ValidationError> validationErrors =
                AppInstaller::Manifest::ValidateManifest(manifestItem);

            int errors = 0;
            for (auto& error : validationErrors)
            {
                if (error.ErrorLevel == Manifest::ValidationError::Level::Error)
                {
                    AICLI_LOG(Repo, Error, << "Received manifest contains validation error: " << error.Message);
                    errors++;
                }
            }

            THROW_HR_IF(APPINSTALLER_CLI_ERROR_RESTSOURCE_INVALID_DATA, errors > 0);

            results.emplace_back(manifestItem);
        }

        return results;
    }
}
