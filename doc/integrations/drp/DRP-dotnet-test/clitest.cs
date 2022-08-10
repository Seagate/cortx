using System;
using System.Collections.Generic;
using System.Data;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using ADHDTech.DRP;
using McMaster.Extensions.CommandLineUtils;
using Newtonsoft.Json.Linq;

namespace DRP_dotnet_test
{
    class clitest
    {
        static void Main(string[] args)
        {
            // clitest.exe <-g|--greeting|-$ <greeting>> [name <fullname>]
            // [-?|-h|--help] [-u|--uppercase]

            CommandLineApplication commandLineApplication = new CommandLineApplication();

            CommandOption alias = commandLineApplication.Option(
              "-a | --Alias <alias>",
              "Alias for new connection.",
              CommandOptionType.SingleValue);

            CommandOption url = commandLineApplication.Option(
              "-e | --URL <address>",
              "URL for connection.",
              CommandOptionType.SingleValue);

            CommandOption user = commandLineApplication.Option(
              "-u | --User <user>",
              "Username for connection.",
              CommandOptionType.SingleValue);

            CommandOption pass = commandLineApplication.Option(
              "-p | --Pass <password>",
              "Password for connection.",
              CommandOptionType.SingleValue);

            CommandOption proxyAddress = commandLineApplication.Option(
              "-l | --ProxyAddress <address>",
              "Proxy URL for connection.",
              CommandOptionType.SingleValue);

            CommandOption proxyUser = commandLineApplication.Option(
              "-m | --ProxyUser <user>",
              "Proxy user for connection.",
              CommandOptionType.SingleValue);

            CommandOption proxyPass = commandLineApplication.Option(
              "-n | --ProxyPass <password>",
              "Proxy password for connection.",
              CommandOptionType.SingleValue);

            CommandOption timeout = commandLineApplication.Option(
              "-t | --Timeout <timeout>",
              "Timeout for connection.",
              CommandOptionType.SingleValue);

            commandLineApplication.HelpOption("-? | -h | --help");

            commandLineApplication.OnExecute(() =>
            {
                if (alias.HasValue() && url.HasValue())
                {
                    int? timeoutValue = null;
                    if (timeout.HasValue()) {
                        timeoutValue = Int32.Parse(timeout.Value());
                    }
                    TestDRPConnection(alias.Value(), url.Value(), user.Value(), pass.Value(), proxyAddress.Value(), proxyUser.Value(), proxyAddress.Value(), timeoutValue);
                    //Console.ReadKey();
                }
                else {
                    commandLineApplication.ShowHelp();
                }
                return 0;
            });

            try
            {
                commandLineApplication.ExecuteAsync(args);
                System.Threading.Thread.Sleep(1000);
            }
            catch (Exception ex)
            {
                // Exception
                //commandLineApplication.ShowHelp();
                Console.WriteLine(ex.Message);
            }

            //Console.Read();
        }

        private static void TestDRPConnection(string alias, string url, string user, string pass, string proxyAddress, string proxyUser, string proxyPass, int? timeout)
        {
            BrokerProfile thisBrokerProfile = new BrokerProfile();
            Console.WriteLine("Setting: {0} to {1}", alias, url);
            BrokerProfile testBrokerProfile = new BrokerProfile
            {
                Alias = alias,
                URL = url,
                User = user ?? "",
                Pass = pass ?? "",
                ProxyAddress = proxyAddress ?? "",
                ProxyUser = proxyUser ?? "",
                ProxyPass = proxyPass ?? "",
                Timeout = timeout ?? 3000
            };

            //string[] testPath = new string[] { "Mesh", "Services" };
            string[] testPath = new string[] { };

            DRP_Client myDRPClient = new DRP_Client(testBrokerProfile);
            if (!myDRPClient.Open().GetAwaiter().GetResult())
            {
                Console.WriteLine("Could not open connection to Broker");
                return;
            }
            else {
                Console.WriteLine("Connected to DRP Broker!");
            }

            JObject returnedData = myDRPClient.SendCmd_Async("DRP", "pathCmd", new Dictionary<string, object>() { { "method", "cliGetPath" }, { "pathList", testPath }, { "listOnly", true } }).GetAwaiter().GetResult();

            if (returnedData != null)
            {
                Console.WriteLine(returnedData.ToString());
            }
            else {
                Console.WriteLine("No data found");
            }

            myDRPClient.CloseSession();
        }
    }
}
