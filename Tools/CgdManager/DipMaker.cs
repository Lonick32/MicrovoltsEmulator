using System;
using System.IO;
using Ionic.Zip;
using System.Text;

class Program
{
    static int Main(string[] args)
    {
        if (args.Length < 1)
        {
            Console.Error.WriteLine("Usage: DipMaker <dirPath> [password]");
            return 1;
        }

        string dirPath = args[0];
        string passwordArg = args.Length > 1 ? args[1] : string.Empty;

        if (!Directory.Exists(dirPath))
        {
            Console.Error.WriteLine($"Invalid path: {dirPath}");
            return 2;
        }

        Console.WriteLine("Creating new cgd.dip archive...");

        string output = Path.Combine(Path.GetDirectoryName(dirPath), "cgd.dip");

        try
        {
            using (var zip = new ZipFile())
            {
                zip.CompressionLevel = Ionic.Zlib.CompressionLevel.BestCompression;

                string password = string.IsNullOrEmpty(passwordArg)
                                  ? "defaultpassword"
                                  : passwordArg;

                zip.Password = password;

                foreach (string f in Directory.GetFiles(dirPath, "*", SearchOption.AllDirectories))
                {
                    if (Path.GetExtension(f).Equals(".dip", StringComparison.OrdinalIgnoreCase))
                        continue;

                    string relativeDir = Path.GetRelativePath(dirPath, Path.GetDirectoryName(f));

                    zip.AddFile(f, relativeDir);
                }

                zip.Save(output);
            }

            Console.WriteLine($"Archive saved: {output}");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("Error: " + ex.Message);
            return 3;
        }
    }
}
