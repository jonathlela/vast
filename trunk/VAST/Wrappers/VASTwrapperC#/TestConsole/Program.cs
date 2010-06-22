
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Runtime.InteropServices; // to call the C DLL via DllImport

namespace ConsoleApplication1
{
    public class Program
    {
        // make sure LoadLibrary is usable 
        [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
        static extern IntPtr LoadLibrary(string lpFileName);

        // load functions exported by VASTwrapperC.dll
        [DllImport("VASTwrapperC")]
        public static extern int InitVAST(bool is_gateway, String str);

        [DllImport("VASTwrapperC")]
        public static extern int ShutVAST();

        [DllImport("VASTwrapperC")]
        public static extern bool VASTJoin(float x, float y, ushort radius);

        [DllImport("VASTwrapperC")]
        public static extern bool VASTLeave ();

        [DllImport("VASTwrapperC")]
        public static extern bool VASTMove(float x, float y);

        [DllImport("VASTwrapperC")]
        public static extern uint VASTTick (uint time_budget);

        [DllImport("VASTwrapperC")]
        public static extern bool VASTPublish (String msg, uint size, ushort radius);

        [DllImport("VASTwrapperC")]
        //public static extern bool VASTReceive (ref String msg, ref uint size, ref UInt64 from);
        public static extern IntPtr VASTReceive(ref uint size, ref UInt64 from);

        [DllImport("VASTwrapperC")]
        public static extern bool isVASTInit ();

        [DllImport("VASTwrapperC")]
        public static extern bool isVASTJoined();

        [DllImport("VASTwrapperC")]
        public static extern ulong VASTGetSelfID();

        public static void Main(string[] args)
        {
            Console.WriteLine("hello world");

            // Ensure current directory is exe directory
            //Environment.CurrentDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);

            //string dllPath = Path.GetFullPath(@".\VASTwrapperC.dll");
            //LoadLibrary(dllPath);

            //Console.WriteLine(MyFunc(5));

            // set default
            string IP = "127.0.0.1";
            float g_x, g_y;
            g_x = g_y = 100;
            //ushort port = 1037;
            string port = "1037";

            bool is_gateway = true;

            // read parameters
            if (args.Length >= 1)
            {
                //port = Convert.ToUInt16(args[1]);
                port = args[0];
            }
            if (args.Length >= 2)
            {
                IP = args[1];
                is_gateway = false;
            }

            string gateway = IP + ":" + port;

            Console.WriteLine("gateway: " + gateway);

            try
            {
                InitVAST(is_gateway, gateway);

                VASTJoin(g_x, g_y, 200);
            }
            catch (DllNotFoundException e)
            {
                Console.WriteLine(e.ToString());
            }
            catch (EntryPointNotFoundException e)
            {
                Console.WriteLine(e.ToString());
            }

            bool g_finished = false;
            uint tick_count = 0;

            // infinite loop to do chat
            while (!g_finished)
            {
                tick_count++;

                // perform tick and process for as long as necessary
                VASTTick(0);

                //getInput ();
                VASTMove(g_x, g_y);

                // check for any received message & print it out
                UInt64 from = 0;
                uint size = 0;

                while (true)
                {
                    IntPtr ptr = VASTReceive(ref size, ref from);

                    if (size == 0 && from == 0)
                        break;

                    //Console.WriteLine(sBuffer.ToString ());                    
                    string str = Marshal.PtrToStringAnsi(ptr);

                    Console.WriteLine("size: " + size + " msg: " + str);
                }

                // sleep a little (for 100ms)
                System.Threading.Thread.Sleep(100);

                // say something every 10 ticks
                if (tick_count % 10 == 0)
                {
                    ulong id = VASTGetSelfID();
                    string msg = id + ": How are you world?";
                    VASTPublish(msg, (uint)msg.Length, 0);
                }
            }

            VASTLeave ();
            ShutVAST ();
        }
    }
}

