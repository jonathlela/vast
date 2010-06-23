
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
        public static extern IntPtr VASTReceive(ref uint size, ref UInt64 from);

        [DllImport("VASTwrapperC")]
        public static extern bool isVASTInit ();

        [DllImport("VASTwrapperC")]
        public static extern bool isVASTJoined();

        [DllImport("VASTwrapperC")]
        public static extern ulong VASTGetSelfID();


        static public float  g_x, g_y;
        static public bool   g_is_gateway = true;
        static public string g_gateway;

        static public void init()
        {
            Console.WriteLine("init called");

            // Ensure current directory is exe directory
            //Environment.CurrentDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);

            //string dllPath = Path.GetFullPath(@".\VASTwrapperC.dll");
            //LoadLibrary(dllPath);

            g_x = g_y = 100;

            try
            {
                InitVAST(g_is_gateway, g_gateway);
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
        }

        static public void loop()
        {
            //bool g_finished = false;
            uint tick_count = 0;

            // infinite loop to do chat
            while (tick_count <50)
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
                    string msg = id + " :message test";
                    VASTPublish(msg, (uint)msg.Length, 0);
                }
            }
        }

        static public void shutdown()
        {
            VASTLeave();
            ShutVAST();
        }

        public static void Main(string[] args)
        {
            // set default
            string IP = "127.0.0.1";
            string port = "1037";
                        
            // read parameters
            if (args.Length >= 1)
            {
                //port = Convert.ToUInt16(args[1]);
                port = args[0];
            }
            if (args.Length >= 2)
            {
                IP = args[1];
                g_is_gateway = false;
            }

            g_gateway = IP + ":" + port;
            Console.WriteLine("gateway: " + g_gateway);

            while (true)
            {
                init();
                loop();

                // only shutdown non-gateway
                if (g_is_gateway == false)
                {
                    shutdown();

                    // some delay is needed so certain network resources can be released properly
                    System.Threading.Thread.Sleep(2000);
                }
            }
        }
    }
}

