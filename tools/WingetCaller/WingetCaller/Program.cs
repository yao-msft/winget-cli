using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Management.Deployment;

namespace WingetCaller
{

    /*
    const CLSID WINGET_INPROC_COM_CLSID_PackageManager = { 0x2DDE4456, 0x64D9, 0x4673, 0x8F, 0x7E, 0xA4, 0xF1, 0x9A, 0x2E, 0x6C, 0xC3 }; // 2DDE4456-64D9-4673-8F7E-A4F19A2E6CC3
    const CLSID WINGET_INPROC_COM_CLSID_FindPackagesOptions = { 0x96B9A53A, 0x9228, 0x4DA0, 0xB0, 0x13, 0xBB, 0x1B, 0x20, 0x31, 0xAB, 0x3D }; // 96B9A53A-9228-4DA0-B013-BB1B2031AB3D
    const CLSID WINGET_INPROC_COM_CLSID_CreateCompositePackageCatalogOptions = { 0x768318A6, 0x2EB5, 0x400D, 0x84, 0xD0, 0xDF, 0x35, 0x34, 0xC3, 0x0F, 0x5D }; // 768318A6-2EB5-400D-84D0-DF3534C30F5D
    const CLSID WINGET_INPROC_COM_CLSID_InstallOptions = { 0xE2AF3BA8, 0x8A88, 0x4766, 0x9D, 0xDA, 0xAE, 0x40, 0x13, 0xAD, 0xE2, 0x86 }; // E2AF3BA8-8A88-4766-9DDA-AE4013ADE286
    const CLSID WINGET_INPROC_COM_CLSID_UninstallOptions = { 0x869CB959, 0xEB54, 0x425C, 0xA1, 0xE4, 0x1A, 0x1C, 0x29, 0x1C, 0x64, 0xE9 }; // 869CB959-EB54-425C-A1E4-1A1C291C64E9
    const CLSID WINGET_INPROC_COM_CLSID_PackageMatchFilter = { 0x57DC8962, 0x7343, 0x42CD, 0xB9, 0x1C, 0x04, 0xF6, 0xA2, 0x5D, 0xB1, 0xD0 }; // 57DC8962-7343-42CD-B91C-04F6A25DB1D0
    */


    class Program
    {
        /*[UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void Callback(string url, string path, byte[] hash);
        */

        static DownloadCallback mycb = (url, path, hash) =>
        {
            using (var client = new WebClient())
            {
                try
                {
                    ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
                    client.DownloadFile(url, path);
                }
                catch (Exception e)
                {
                    var msg = e.Message;
                }
            }
        };

        static void Main(string[] args)
        {
            // initialize PackageManagerSettings
            var pmsGuid = Guid.Parse("80CF9D63-5505-4342-B9B4-BB87895CA8BB");
            var pmsType = Type.GetTypeFromCLSID(pmsGuid);
            Console.WriteLine(pmsType.AssemblyQualifiedName);
            var pms = (PackageManagerSettings)Activator.CreateInstance(pmsType);
            pms.SetCallerIdentifier("wingetcaller");
            pms.SetStateIdentifier("wingetcaller");
            pms.SetUserSettings(
                @"{""$schema"": ""https://aka.ms/winget-settings.schema.json"", ""source"": {""autoUpdateIntervalInMinutes"": 3},}"
            );

            // now initialize PackageManager to do work
            var pmGuid = Guid.Parse("2DDE4456-64D9-4673-8F7E-A4F19A2E6CC3");
            var pmType = Type.GetTypeFromCLSID(pmGuid);
            var pm = (PackageManager)Activator.CreateInstance(pmType);
            var owcSource = pm.GetPredefinedPackageCatalog(PredefinedPackageCatalog.OpenWindowsCatalog);

            var fpoGuid = Guid.Parse("96B9A53A-9228-4DA0-B013-BB1B2031AB3D");
            var fpoType = Type.GetTypeFromCLSID(fpoGuid);
            var fpo = (FindPackagesOptions)Activator.CreateInstance(fpoType);

            var pmfGuid = Guid.Parse("57DC8962-7343-42CD-B91C-04F6A25DB1D0");
            var pmfType = Type.GetTypeFromCLSID(pmfGuid);
            var pmf = (PackageMatchFilter)Activator.CreateInstance(pmfType);

            pmf.Field = PackageMatchField.Id;
            pmf.Option = PackageFieldMatchOption.Equals;
            pmf.Value = args.Length > 0 ? args[0] : "VideoLAN.VLC";
            fpo.Filters.Add(pmf);

            var ccpcoGuid = Guid.Parse("768318A6-2EB5-400D-84D0-DF3534C30F5D");
            var ccpcoType = Type.GetTypeFromCLSID(ccpcoGuid);
            var ccpco = (CreateCompositePackageCatalogOptions)Activator.CreateInstance(ccpcoType);

            ccpco.Catalogs.Add(owcSource);
            ccpco.CompositeSearchBehavior = CompositeSearchBehavior.AllCatalogs;

            var compositeSource = pm.CreateCompositePackageCatalog(ccpco).Connect().PackageCatalog;

            var searchResult = compositeSource.FindPackages(fpo).Matches;
            Console.WriteLine("Found app count: " + searchResult.Count);

            if (searchResult.Count > 0)
            {
                var p = searchResult.ElementAt(0).CatalogPackage;
                Console.WriteLine(p.Name);

                if (p.InstalledVersion != null)
                {
                    Console.WriteLine("installed version: " + p.InstalledVersion.Version);
                }

                var pioGuid = Guid.Parse("E2AF3BA8-8A88-4766-9DDA-AE4013ADE286");
                var pioType = Type.GetTypeFromCLSID(pioGuid);
                var pio = (InstallOptions)Activator.CreateInstance(pioType);

                pio.PackageInstallMode = PackageInstallMode.Default;
                var op = pm.InstallPackageAsync(p, pio);

                pio.DownloadCallback = mycb;

                Console.WriteLine("Started Install");

                while (op.Status != Windows.Foundation.AsyncStatus.Completed
                    && op.Status != Windows.Foundation.AsyncStatus.Error)
                {
                    Console.WriteLine("Installing...");
                    Thread.Sleep(1000);
                }

                if (op.Status == Windows.Foundation.AsyncStatus.Completed)
                {
                    if (op.GetResults().Status == InstallResultStatus.Ok)
                    {
                        Console.WriteLine("Success");
                    }
                    else
                    {
                        Console.WriteLine("Fail");
                    }
                }
                else
                {
                    Console.WriteLine("Fail");
                }
            }
        }
    }
}
