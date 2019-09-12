using System;
using System.IO;
using System.Text;

namespace OperaTool
{
  internal class Program
  {
    private static void Main(string[] args)
    {
      Opera opera = new Opera();
      if (opera == null)
        return;

      if (args != null && args.Length == 3)
      {
        if (args[0].Equals("-d", StringComparison.OrdinalIgnoreCase) == true)
        {
          string filename = args[1];
          string path = args[2];

          if (filename != String.Empty && path != string.Empty)
          {
            opera.DecompileFS(filename, path);

            //opera.CompileFSFromMemory("compile.iso");
          }
          else
            ShowHelp();
        }
        else if (args[0].Equals("-c", StringComparison.OrdinalIgnoreCase) == true)
        {
          string path = args[1];
          string filename = args[2];

          if (path != String.Empty && filename != string.Empty)
          {
            opera.CompileFS(path, filename);
          }
          else
            ShowHelp();
        }
      }
      else
        ShowHelp();
    }

    private static void ShowHelp()
    {
      Console.WriteLine("OperaTool 0.01a - By Cristina Ramos");
      Console.WriteLine("Usage:");
      Console.WriteLine("(Decompile disc) OperaTool -d filename.iso path");
      Console.WriteLine("(Compile disc)   OperaTool -c path filename.iso");
    }
  }
}
