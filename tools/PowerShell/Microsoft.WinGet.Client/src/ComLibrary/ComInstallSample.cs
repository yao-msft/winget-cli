using Microsoft.Management.Deployment;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Windows.Devices.Input;

namespace ComLibrary
{
    [Cmdlet(VerbsCommon.Add, "WingetPackage")]
    [OutputType(typeof(InstallResult))]
    public class ComInstallSample : Cmdlet
    {
        [Parameter(Position = 0, ValueFromPipeline = true, ValueFromPipelineByPropertyName = true)]
        [ValidateNotNullOrEmpty]
        [Alias("PackageId", "Id")]
        public string PackageIdentifier { get; set; }


        protected override void BeginProcessing()
        {
            base.BeginProcessing();
            WriteVerbose("Here0");
        }

        protected override void ProcessRecord()
        {
            WriteVerbose("Here1");
            var pmGuid = Guid.Parse(Constants.ClsidPackageManager);
            var pmType = Type.GetTypeFromProgID("WingetDev.PackageManager");
            WriteVerbose("Here2");
            var pm = (PackageManager)Activator.CreateInstance(pmType);
            WriteVerbose("Here3");
            var owcSource = pm.GetPredefinedPackageCatalog(PredefinedPackageCatalog.OpenWindowsCatalog).Connect().PackageCatalog;
            InstallResult result = new InstallResult();
            result.PackageInstallResult = false;

            var fpoGuid = Guid.Parse(Constants.ClsidFindPackagesOptions);
            var fpoType = Type.GetTypeFromCLSID(fpoGuid);
            var fpo = (FindPackagesOptions)Activator.CreateInstance(fpoType);

            var pmfGuid = Guid.Parse(Constants.ClsidPackageMatchFilter);
            var pmfType = Type.GetTypeFromCLSID(pmfGuid);
            var pmf = (PackageMatchFilter)Activator.CreateInstance(pmfType);

            pmf.Field = PackageMatchField.Id;
            pmf.Option = PackageFieldMatchOption.Equals;
            pmf.Value = PackageIdentifier;
            fpo.Filters.Add(pmf);

            var searchResult = owcSource.FindPackages(fpo).Matches;
            WriteVerbose("Find package with Package Identifier " + PackageIdentifier + " result. Count: " + searchResult.Count);

            if (searchResult.Count > 0)
            {
                var p = searchResult.ElementAt(0).CatalogPackage;
                WriteVerbose("Package Name:" + p.Name);

                var pioGuid = Guid.Parse(Constants.ClsidInstallOptions);
                var pioType = Type.GetTypeFromCLSID(pioGuid);
                var pio = (InstallOptions)Activator.CreateInstance(pioType);

                pio.PackageInstallMode = PackageInstallMode.Silent;
                var op = pm.InstallPackageAsync(p, pio);

                while (op.Status != Windows.Foundation.AsyncStatus.Completed
                    && op.Status != Windows.Foundation.AsyncStatus.Error)
                {
                    WriteVerbose("Installing...");
                    Thread.Sleep(1000);
                }

                if (op.Status == Windows.Foundation.AsyncStatus.Completed && op.GetResults().Status == InstallResultStatus.Ok)
                {
                    result.PackageInstallResult = true;
                }

                WriteVerbose("InstallationResult: " + result.PackageInstallResult);
            }
            else
            {
                WriteVerbose("Package with Package Identifier " + PackageIdentifier + " not found.");
            }

            WriteObject(result);
        }
    }

    public class Constants
    {
        private const bool useDev = true;
        public const string ClsidPackageManager = useDev ? "74CB3139-B7C5-4B9E-9388-E6616DEA288C" : "C53A4F16-787E-42A4-B304-29EFFB4BF597";
        public const string ClsidFindPackagesOptions = useDev ? "1BD8FF3A-EC50-4F69-AEEE-DF4C9D3BAA96" : "572DED96-9C60-4526-8F92-EE7D91D38C1A";
        public const string ClsidCreateCompositePackageCatalogOptions = useDev ? "EE160901-B317-4EA7-9CC6-5355C6D7D8A7" : "526534B8-7E46-47C8-8416-B1685C327D37";
        public const string ClsidInstallOptions = useDev ? "44FE0580-62F7-44D4-9E91-AA9614AB3E86" : "1095F097-EB96-453B-B4E6-1613637F3B14";
        public const string ClsidUninstallOptions = useDev ? "AA2A5C04-1AD9-46C4-B74F-6B334AD7EB8C" : "E1D9A11E-9F85-4D87-9C17-2B93143ADB8D";
        public const string ClsidPackageMatchFilter = useDev ? "3F85B9F4-487A-4C48-9035-2903F8A6D9E8" : "D02C9DAF-99DC-429C-B503-4E504E4AB000";
    }

    public class DevConstants
    {
        public const string ClsidPackageManager = "74CB3139-B7C5-4B9E-9388-E6616DEA288C";
        public const string ClsidFindPackagesOptions = "572DED96-9C60-4526-8F92-EE7D91D38C1A";
        public const string ClsidCreateCompositePackageCatalogOptions = "526534B8-7E46-47C8-8416-B1685C327D37";
        public const string ClsidInstallOptions = "1095F097-EB96-453B-B4E6-1613637F3B14";
        public const string ClsidUninstallOptions = "E1D9A11E-9F85-4D87-9C17-2B93143ADB8D";
        public const string ClsidPackageMatchFilter = "D02C9DAF-99DC-429C-B503-4E504E4AB000";
    }

    public class InstallResult
    {
        public bool PackageInstallResult { get; set; }
    }
}
