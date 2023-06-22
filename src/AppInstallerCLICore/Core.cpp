// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"
#include "Public/AppInstallerCLICore.h"
#include "Commands/RootCommand.h"
#include "ExecutionContext.h"
#include "Workflows/WorkflowBase.h"
#include <winget/UserSettings.h>
#include "Commands/InstallCommand.h"
#include "COMContext.h"
#include <AppInstallerFileLogger.h>
#include <wil/result_macros.h>
#include <inspectable.h>
#include <wrl/wrappers/corewrappers.h>
#include "AuthProto.h"

#ifndef AICLI_DISABLE_TEST_HOOKS
#include <winget/Debugging.h>
#endif
#include <windows.ui.core.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Security::Authentication::Web::Core;
using namespace winrt::Windows::Security::Credentials;
using namespace AppInstaller::CLI;
using namespace AppInstaller::Utility::literals;

namespace AppInstaller::CLI
{
    namespace
    {
        // RAII class to restore the console output codepage.
        struct ConsoleOutputCPRestore
        {
            ConsoleOutputCPRestore(UINT cpToChangeTo)
            {
                m_previousCP = GetConsoleOutputCP();
                LOG_LAST_ERROR_IF(!SetConsoleOutputCP(cpToChangeTo));
            }

            ~ConsoleOutputCPRestore()
            {
                SetConsoleOutputCP(m_previousCP);
            }

            ConsoleOutputCPRestore(const ConsoleOutputCPRestore&) = delete;
            ConsoleOutputCPRestore& operator=(const ConsoleOutputCPRestore&) = delete;

            ConsoleOutputCPRestore(ConsoleOutputCPRestore&&) = delete;
            ConsoleOutputCPRestore& operator=(ConsoleOutputCPRestore&&) = delete;

        private:
            UINT m_previousCP = 0;
        };
    }

    HWND GetConsoleHwnd(void)
    {
#define MY_BUFSIZE 1024 // Buffer size for console window titles.
        HWND hwndFound;         // This is what is returned to the caller.
        wchar_t pszNewWindowTitle[MY_BUFSIZE]; // Contains fabricated
        // WindowTitle.
        wchar_t pszOldWindowTitle[MY_BUFSIZE]; // Contains original
        // WindowTitle.

// Fetch current window title.

        GetConsoleTitle(pszOldWindowTitle, MY_BUFSIZE);

        // Format a "unique" NewWindowTitle.

        wsprintf(pszNewWindowTitle, L"%d/%d",
            (int)GetTickCount64(),
            GetCurrentProcessId());

        // Change current window title.

        SetConsoleTitle(pszNewWindowTitle);

        // Ensure window title has been updated.

        Sleep(40);

        // Look for NewWindowTitle.

        hwndFound = FindWindow(NULL, pszNewWindowTitle);

        // Restore original window title.

        SetConsoleTitle(pszOldWindowTitle);

        return(hwndFound);
    }

    static LRESULT __stdcall WndProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
    {
        WINRT_ASSERT(window);

        

        return DefWindowProc(window, message, wparam, lparam);
    }

    int CoreMain(int, wchar_t const**)
    {
        init_apartment();

#ifndef AICLI_DISABLE_TEST_HOOKS
        if (Settings::User().Get<Settings::Setting::EnableSelfInitiatedMinidump>())
        {
            Debugging::EnableSelfInitiatedMinidump();
        }
#endif

        Logging::UseGlobalTelemetryLoggerActivityIdOnly();

        Execution::Context context{ std::cout, std::cin };
        auto previousThreadGlobals = context.SetForCurrentThread();
        context.EnableCtrlHandler();

        // Enable all logging for this phase; we will update once we have the arguments
        Logging::Log().EnableChannel(Logging::Channel::All);
        Logging::Log().SetLevel(Settings::User().Get<Settings::Setting::LoggingLevelPreference>());
        Logging::FileLogger::Add();
        Logging::EnableWilFailureTelemetry();

        // Set output to UTF8
        ConsoleOutputCPRestore utf8CP(CP_UTF8);

        

        Logging::Telemetry().SetCaller("winget-cli");
        Logging::Telemetry().LogStartup();

        // Initiate the background cleanup of the log file location.
        Logging::FileLogger::BeginCleanup();

        context << Workflow::ReportExecutionStage(Workflow::ExecutionStage::ParseArgs);


   //     std::thread waitThread([&]
   //         {
                const wchar_t CLASS_NAME[] = L"Sample Window Class";

                HMODULE resourceModule = nullptr;
                GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<PCWSTR>(CoreMain),
                    &resourceModule);
                THROW_LAST_ERROR_IF_NULL(resourceModule);

                WNDCLASS wc = { };

                wc.lpfnWndProc = WndProc;
                wc.hInstance = resourceModule;
                wc.lpszClassName = CLASS_NAME;

                RegisterClass(&wc);

                HWND hwnd = CreateWindowEx(
                    0,                              // Optional window styles.
                    CLASS_NAME,                     // Window class
                    L"Learn to Program Windows",    // Window text
                    WS_OVERLAPPEDWINDOW,            // Window style

                    // Size and position
                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

                    NULL,       // Parent window    
                    NULL,       // Menu
                    resourceModule,  // Instance handle
                    NULL        // Additional application data
                );

                if (hwnd == NULL)
                {
                    return 0;
                }

               auto provider = WebAuthenticationCoreManager::FindAccountProviderAsync(L"https://login.microsoft.com", L"organizations").get();

                WebTokenRequest req(provider, L"", L"edb7e0dc-a3bf-4b99-a0aa-6cad61ed1b5e");

                req.Properties().Insert(L"resource", L"https://onestore.microsoft.com");

                winrt::Windows::Foundation::IAsyncOperation<WebTokenRequestResult> requestOperation;

                constexpr winrt::guid iidAsyncRequestResult{ winrt::guid_of<IAsyncOperation<WebTokenRequestResult>>() };

                auto managerFactory = winrt::get_activation_factory<WebAuthenticationCoreManager>();
                winrt::com_ptr<IWebAuthenticationCoreManagerInterop> managerInterop{ managerFactory.as<IWebAuthenticationCoreManagerInterop>() };

                auto hr = managerInterop->RequestTokenForWindowAsync(GetForegroundWindow(), req.as<::IInspectable>().get(), iidAsyncRequestResult, reinterpret_cast<void**>(&requestOperation));

                if (SUCCEEDED(hr))
                {
                    auto result = requestOperation.get();
                    if (result.ResponseStatus() != WebTokenRequestStatus::Success)
                    {
                        std::cout << result.ResponseError().ErrorCode() << std::endl;
                        std::wcout << result.ResponseError().ErrorMessage().c_str();
                        return -1;
                    }
                    else
                    {
                        std::wcout << result.ResponseData().GetAt(0).Token().c_str();
                    }
                }

               // TryAuthProto();


              //  RuntimeClass_Windows_UI_Core_CoreWindow

//                return 0;
  //          });
        
 //       waitThread.join();

        return 0;

        /*// Convert incoming wide char args to UTF8
        std::vector<std::string> utf8Args;
        for (int i = 1; i < argc; ++i)
        {
            utf8Args.emplace_back(Utility::ConvertToUTF8(argv[i]));
        }

        AICLI_LOG(CLI, Info, << "WinGet invoked with arguments:" << [&]() {
            std::stringstream strstr;
            for (const auto& arg : utf8Args)
            {
                strstr << " '" << arg << '\'';
            }
            return strstr.str();
            }());

        Invocation invocation{ std::move(utf8Args) };

        // The root command is our fallback in the event of very bad or very little input
        std::unique_ptr<Command> command = std::make_unique<RootCommand>();

        try
        {
            std::unique_ptr<Command> subCommand = command->FindSubCommand(invocation);
            while (subCommand)
            {
                command = std::move(subCommand);
                subCommand = command->FindSubCommand(invocation);
            }
            Logging::Telemetry().LogCommand(command->FullName());

            command->ParseArguments(invocation, context.Args);

            // Change logging level to Info if Verbose not requested
            if (context.Args.Contains(Execution::Args::Type::VerboseLogs))
            {
                Logging::Log().SetLevel(Logging::Level::Verbose);
            }

            context.UpdateForArgs();
            context.SetExecutingCommand(command.get());
            command->ValidateArguments(context.Args);
        }
        // Exceptions specific to parsing the arguments of a command
        catch (const CommandException& ce)
        {
            command->OutputHelp(context.Reporter, &ce);
            AICLI_LOG(CLI, Error, << "Error encountered parsing command line: " << ce.Message());
            return APPINSTALLER_CLI_ERROR_INVALID_CL_ARGUMENTS;
        }
        catch (const Settings::GroupPolicyException& e)
        {
            // Report any action blocked by Group Policy.
            auto policy = Settings::TogglePolicy::GetPolicy(e.Policy());
            AICLI_LOG(CLI, Error, << "Operation blocked by Group Policy: " << policy.RegValueName());
            context.Reporter.Error() << Resource::String::DisabledByGroupPolicy(policy.PolicyName()) << std::endl;
            return APPINSTALLER_CLI_ERROR_BLOCKED_BY_POLICY;
        }

        return Execute(context, command);

        // End of the line exceptions that are not ever expected.
        // Telemetry cannot be reliable beyond this point, so don't let these happen.*/
    }

    void ServerInitialize()
    {
        AppInstaller::CLI::Execution::COMContext::SetLoggers();
    }
}
