using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Text;

namespace rec_cpp_gen
{
    static class Program
    {
        /// <summary>
        ///  The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            var text = File.ReadAllText("C:\\reicast-emulator\\build\\fnlist.txt");


            var lines = text.Split("\r\n", StringSplitOptions.RemoveEmptyEntries);
            Dictionary<string, int> names = new Dictionary<string, int>();
            int name = 0;

            StringBuilder sb = new StringBuilder();
            StringBuilder sb2 = new StringBuilder();

            foreach (var line in lines)
            {
                names[line] = ++name;
                var items = line.Split(";", StringSplitOptions.RemoveEmptyEntries);

                sb.Append("struct block_" + name + @" : fnblock_base {
	opcodeExec base_ptr[0];
	
	void execute()
	{
		cycle_counter -= cc;
		auto ptr_bytes = reinterpret_cast<u8*>(base_ptr);
");

                foreach(var item in items)
                {
                    sb.AppendLine("reinterpret_cast <" + item + "*>(ptr_bytes)->execute();");
                    sb.AppendLine("ptr_bytes += sizeof(" + item + ");");
                }

                sb.AppendLine(
@"
		
	}

	static void* create(void* buffer, int cc) {
		auto rv = new (buffer) block_" + name + @"();
		rv->cc = cc;
		return rv;
	}
};");
                sb2.AppendLine("(*dest)[\"" + line + "\"] = &block_" + name + "::create;");
                sb2.AppendLine("(*dest)[\"" + line.Replace("struct ", "") + "\"] = &block_" + name + "::create;");

                if (name % 100 == 0)
                {
                    sb.AppendLine("void init_blocks_" + name + "(map<string, void* (*)(void*, int)>* dest) {");

                    if (name != 100)
                    {
                        sb.AppendLine("void init_blocks_" + (name - 100) + "(map<string, void* (*)(void*, int)>* dest);");
                        sb.AppendLine("init_blocks_" + (name - 100) + "(dest);");
                    }
                    
                    sb.Append(sb2.ToString());
                    sb.AppendLine("}");

                    File.WriteAllText("C:\\reicast-emulator\\build\\blocks_impls" + name + ".cpp", 
                        "#include \"rec_cpp.inl\"\n" + sb.ToString());
                    
                    sb.Clear();
                    sb2.Clear();
                }
            }
            File.WriteAllText("C:\\reicast-emulator\\build\\blocks_impls.final.inl", sb.ToString());
            File.WriteAllText("C:\\reicast-emulator\\build\\blocks_ctors.final.inl", sb2.ToString());

        }
    }
}
