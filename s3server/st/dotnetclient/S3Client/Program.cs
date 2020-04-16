using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using CommandLine;
using CommandLine.Text;

using Amazon;
using Amazon.S3;
using Amazon.S3.Model;

namespace Test
{
    class Options
    {
        [Option('k', "key", Required = false,
          HelpText = "AWS Key id")]
        public String KeyId { get; set; }

        [Option('s', "secret", Required = false,
          HelpText = "AWS secrey key")]
        public String SecretKey { get; set; }

        [ValueList(typeof(List<string>), MaximumElements = 3)]
        public IList<string> CommandItems { get; set; } 

        [HelpOption]
        public string GetUsage()
        {
            var help = new HelpText
            {
                AdditionalNewLineAfterOption = true,
                AddDashesToOption = true
            };
            help.AddPreOptionsLine("Usage:");
            help.AddPreOptionsLine("get s3://<bucket>/<object>");
            help.AddPreOptionsLine("put <object> s3://<bucket>");
            help.AddPreOptionsLine("del s3://<bucket>/<object>");
            help.AddOptions(this);
            return help;
        }
    }

    class Program
    {
        static AmazonS3Config s3Config = new AmazonS3Config()
        {
            ServiceURL = "http://s3.seagate.com",
            UseHttp = true
            //RegionEndpoint = Amazon.RegionEndpoint.USWest2
        };

        static IAmazonS3 s3Client;

        static void Main(string[] args)
        {
            if (args.Length < 2)
            {
                Console.WriteLine("Incorrect command. Use --help option to see the usage");
                Environment.Exit(1);
            }

            var options = new Options();
            if (CommandLine.Parser.Default.ParseArguments(args, options))
            {
                if ( (String.IsNullOrEmpty(options.KeyId) && !String.IsNullOrEmpty(options.SecretKey)) ||
                    (!String.IsNullOrEmpty(options.KeyId) && String.IsNullOrEmpty(options.SecretKey)))
                {
                    Console.WriteLine("The credentials can be specified in the creds file" +
                        " or both AWS key id and secret key have to be given as command line arguments.");
                    Environment.Exit(1);
                }

                /* 
                    Try to fetch the default configuration from profile store
                    if the user doesn't specify the aws key id and aws access key
                    as command line arguments.
                */

                if (String.IsNullOrEmpty(options.KeyId))
                {
                    Dictionary<string, string> creds = ParseAwsCredentials();
                    s3Client = AWSClientFactory.CreateAmazonS3Client(creds["aws_access_key_id"], 
                        creds["aws_secret_access_key"],
                        s3Config);
                }
                else
                {
                    s3Client = AWSClientFactory.CreateAmazonS3Client(
                        options.KeyId, options.SecretKey, s3Config);
                }
            }

            string[] s3Args;
            switch (options.CommandItems[0])
            {
                case "get":
                    s3Args = ParseRequest(options.CommandItems[1]);
                    GetS3Object(s3Args[0], s3Args[1]);
                    break;
                case "put":
                    s3Args = ParseRequest(args[2]);
                    string path = Path.GetFullPath(args[1]);
                    PutS3Object(s3Args[0], Path.GetFileName(path), path);
                    break;
                case "del":
                    s3Args = ParseRequest(args[1]);
                    DeleteS3Object(s3Args[0], s3Args[1]);
                    break;
                default:
                    Console.WriteLine("Incorrect format or operation not supported.");
                    Console.WriteLine("Use --help option to see the usage.");
                    break;
            }
        }

        private static string[] ParseRequest(string request)
        {
            if (!request.StartsWith("s3://"))
            {
                Console.WriteLine(
                    "Incorrect command. Use --help option to see the usage");
                Environment.Exit(1);
            }
    
            string[] temp = request.Split(new string[] { "//" }, StringSplitOptions.None);
            string[] request_components = temp[1].Split(new[] { "/" }, 2, StringSplitOptions.None);
            return request_components;
        }

        private static Dictionary<string, string> ParseAwsCredentials()
        {
            Dictionary<string, string> creds = new Dictionary<string, string>();
            string userProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            string creds_file = Path.Combine(userProfile, ".aws", "credentials");
            string[] temp;

            foreach (string line in File.ReadLines(creds_file))
            {
                temp = line.Split('=');
                if (temp[0] == "aws_access_key_id" || temp[0] == "aws_secret_access_key")
                {
                    creds.Add(temp[0], temp[1]);
                }
            }

            return creds;
        }

        private static void DeleteS3Object(string bucketName, string bucketObject)
        {
            Console.WriteLine("Delete object " + bucketObject);
            DeleteObjectRequest deleteObjectRequest = new DeleteObjectRequest
            {
                BucketName = bucketName,
                Key = bucketObject
            };

            s3Client.DeleteObject(deleteObjectRequest);
            Console.WriteLine("Object deleted");
        }

        private static void PutS3Object(string bucketName, string bucketObject, string fileName)
        {
            try
            {
                string content = System.IO.File.ReadAllText(fileName);
                Console.WriteLine("Uploading file " + fileName);
                PutObjectRequest putRequest1 = new PutObjectRequest
                {
                    BucketName = bucketName,
                    Key = bucketObject,
                    ContentBody = content
                };

                PutObjectResponse response1 = s3Client.PutObject(putRequest1);
                Console.WriteLine("Put object successful");
            }
            catch (AmazonS3Exception amazonS3Exception)
            {
                if (amazonS3Exception.ErrorCode != null &&
                    (amazonS3Exception.ErrorCode.Equals("InvalidAccessKeyId") 
                    || amazonS3Exception.ErrorCode.Equals("InvalidSecurity")))
                {
                    Console.WriteLine("Check the provided AWS Credentials.");
                    Console.WriteLine(
                        "For service sign up go to http://aws.amazon.com/s3");
                }
                else
                {
                    Console.WriteLine(
                        "Error occurred. Message:'{0}' when writing an object"
                        , amazonS3Exception.Message);
                }
            }
        }

        private static void GetS3Object(string bucketName, string bucketObject)
        {
            GetObjectRequest request = new GetObjectRequest
            {
                BucketName = bucketName,
                Key = bucketObject
            };

            using (GetObjectResponse response = s3Client.GetObject(request))
            {
                string dest = Path.Combine(Directory.GetCurrentDirectory(), bucketObject);
                Console.WriteLine("Get object " + bucketObject + " to " + dest);
                if (!File.Exists(dest))
                {
                    response.WriteResponseStreamToFile(dest);
                    Console.WriteLine("File fetched");
                }
                else
                {
                    Console.WriteLine("File exists. Aborting fetch");
                }
            }
        }
    }
}