// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"
#include "TempSQLiteIndexFile.h"

namespace AppInstaller::Repository::Microsoft
{
    namespace
    {
        static constexpr std::string_view s_PreIndexedPackageSourceFactory_IndexFilePath = "Public\\index.db"sv;
    }

    TempSQLiteIndexFile::TempSQLiteIndexFile(AppInstaller::Msix::MsixInfo& packageInfo, AppInstaller::IProgressCallback& progress)
    {
        GUID guid;
        THROW_IF_FAILED(CoCreateGuid(&guid));
        WCHAR tempFileName[256];
        THROW_HR_IF(E_UNEXPECTED, StringFromGUID2(guid, tempFileName, ARRAYSIZE(tempFileName)) == 0);

        m_indexFile = Runtime::GetPathTo(Runtime::PathName::Temp);
        m_indexFile /= tempFileName;

        m_indexHanlde.reset(CreateFileW(m_indexFile.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        THROW_LAST_ERROR_IF(!m_indexHanlde);

        packageInfo.WriteToFile(s_PreIndexedPackageSourceFactory_IndexFilePath, m_indexFile, progress);
    }

    TempSQLiteIndexFile::~TempSQLiteIndexFile()
    {
        if (!m_indexFile.empty() && std::filesystem::exists(m_indexFile))
        {
            if (m_indexHanlde)
            {
                m_indexHanlde.reset();
            }

            try
            {
                std::filesystem::remove(m_indexFile);
            }
            catch (...)
            {
                AICLI_LOG(Repo, Info, << "Failed to remove temp index file at: " << m_indexFile);
            }
        }
    }
}