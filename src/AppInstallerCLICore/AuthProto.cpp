#include "pch.h"
#include <wil/result_macros.h>
#include <inspectable.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.ui.core.h>
#include <wil/result_macros.h>

using namespace ::Windows::Foundation;
using namespace ::Microsoft::WRL;
using namespace ABI::Windows::Foundation;

void TryAuthProto()
{
    ::Microsoft::WRL::ComPtr<ABI::Windows::Security::Authentication::Web::Core::IWebAuthenticationCoreManagerStatics> m_authManagerStatics;
    ::Microsoft::WRL::ComPtr<ABI::Windows::Security::Authentication::Web::Core::IWebTokenRequestFactory> m_tokenFactory;
    ::Microsoft::WRL::ComPtr<ABI::Windows::Security::Credentials::IWebAccountProvider> m_liveProvider;

    HRESULT hr = ::Windows::Foundation::GetActivationFactory(
        ::Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Security_Authentication_Web_Core_WebAuthenticationCoreManager).Get(),
        &m_authManagerStatics);

    if (SUCCEEDED(hr))
    {
        hr = ::Windows::Foundation::GetActivationFactory(
            ::Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Security_Authentication_Web_Core_WebTokenRequest).Get(),
            &m_tokenFactory);
    }

    ::Microsoft::WRL::ComPtr<IWebAuthenticationCoreManagerInterop> managerInterop;
    ::Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<ABI::Windows::Security::Authentication::Web::Core::WebTokenRequestResult*>> webTokenRequestResultOperation;

    hr = m_authManagerStatics.As(&managerInterop);

    ComPtr<IAsyncOperation<ABI::Windows::Security::Credentials::WebAccountProvider*>> findAccountProviderAsyncOperation;

    hr = m_authManagerStatics->FindAccountProviderWithAuthorityAsync(
        ::Microsoft::WRL::Wrappers::HStringReference(L"https://login.microsoft.com").Get(),
        ::Microsoft::WRL::Wrappers::HStringReference(L"organizations").Get(),
        &findAccountProviderAsyncOperation);

    //findAccountProviderAsyncOperation
}