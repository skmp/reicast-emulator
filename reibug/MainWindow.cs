using System;
using System.Net.Sockets;
using System.Threading;
using Gtk;
using MonoUITest;

[Gtk.TreeNode(ListOnly = true)]
public class MyTreeNode : Gtk.TreeNode
{

    string song_title;

    public MyTreeNode(string artist)
    {
        Artist = artist;
        this.song_title = "";
    }

    [Gtk.TreeNodeValue(Column = 0)]
    public string Artist;

    [Gtk.TreeNodeValue(Column = 1)]
    public string SongTitle { get { return song_title; } }
}

public partial class MainWindow : Gtk.Window
{
    bool hasOK;
    bool corePaused;

    TcpClient tcpClient;

    Thread client;

    void CloseSocket()
    {
        if (tcpClient != null)
        {
            lock (tcpClient)
            {
                tcpClient.Close();
                tcpClient.Dispose();
                tcpClient = null;

                client.Join();
            }
        }
    }

    void ClientThread()
    {
        lock(tcpClient)
        {
            hasOK = true;
            corePaused = false;
        }

        if (ReadCommand() != RRI_DebugCommands.RRIBC_Hello)
        {
            CloseSocket();
            return;
        }

        RRI_DebugCommands old_cmd = RRI_DebugCommands.RRIBC_BAD;

        while (tcpClient != null)
        {
            try 
            {
                var command = ReadCommand();

                lock (tcpClient)
                {
                    switch (command)
                    {
                        case RRI_DebugCommands.RRIBC_OK:
                            hasOK = true;
                            break;
                        case RRI_DebugCommands.RRIBC_StepNotification:
                            corePaused = true;
                            break;
                        case RRI_DebugCommands.RRIBC_ClearBuffers:
                            {
                                var paramValue = ReadU32();
                                var depthValue = ReadF32();
                                var stencilValue = ReadU32();

                                Application.Invoke(delegate
                                {
                                    store.AddNode(new MyTreeNode(String.Format(
                                        "ClearBuffers({0}, {1}, {2})",
                                        paramValue,
                                        depthValue,
                                        stencilValue
                                    )), 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_PeelBuffers:
                            {
                                var paramValue = ReadU32();
                                var depthValue = ReadF32();
                                var stencilValue = ReadU32();

                                Application.Invoke(delegate
                                {
                                    store.AddNode(new MyTreeNode(String.Format(
                                        "RRIBC_PeelBuffers({0}, {1}, {2})",
                                        paramValue,
                                        depthValue,
                                        stencilValue
                                    )), 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_SummarizeStencilOr:
                            Application.Invoke(delegate
                            {
                                store.AddNode(new MyTreeNode("RRIBC_SummarizeStencilOr"), 0);
                            });
                            break;
                        case RRI_DebugCommands.RRIBC_SummarizeStencilAnd:
                            Application.Invoke(delegate
                            {
                                store.AddNode(new MyTreeNode("RRIBC_SummarizeStencilAnd"), 0);
                            });
                            break;
                        case RRI_DebugCommands.RRIBC_ClearPixelsDrawn:
                            Application.Invoke(delegate
                            {
                                store.AddNode(new MyTreeNode("RRIBC_ClearPixelsDrawn"), 0);
                            });
                            break;
                        case RRI_DebugCommands.RRIBC_GetPixelsDrawn:
                            {
                                var rv = ReadU32();

                                Application.Invoke(delegate
                                {
                                    store.AddNode(new MyTreeNode("RRIBC_GetPixelsDrawn = " + rv), 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_AddFpuEntry:
                            {
                                var param = ReadDrawParams();

                                var v1 = ReadVertrex();
                                var v2 = ReadVertrex();
                                var v3 = ReadVertrex();

                                var render_mode = ReadU32();

                                var rv = ReadU32();

                                Application.Invoke(delegate
                                {
                                    store.AddNode(new MyTreeNode("RRIBC_AddFpuEntry = " + rv), 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_ClearFpuEntries:
                            Application.Invoke(delegate
                            {
                                store.AddNode(new MyTreeNode("RRIBC_ClearFpuEntries"), 0);
                            });
                            break;
                        case RRI_DebugCommands.RRIBC_GetColorOutputBuffer:
                            {
                                var buffers = ReadBuffers();

                                Application.Invoke(delegate
                                {
                                    store.AddNode(new MyTreeNode("RRIBC_GetColorOutputBuffer"), 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_RenderParamTags:
                            {
                                var rm = ReadU32();
                                var tileX = ReadU32();
                                var tileY = ReadU32();

                                Application.Invoke(delegate
                                {
                                    store.AddNode(new MyTreeNode("RRIBC_RenderParamTags"), 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_RasterizeTriangle:
                            {
                                var render_mode = ReadU32();
                                var param = ReadDrawParams();
                                var tag = ReadU32();
                                var vertex_offset = ReadU32();

                                var v1 = ReadVertrex();
                                var v2 = ReadVertrex();
                                var v3 = ReadVertrex();

                                var has_v4 = ReadU8();

                                if (has_v4 != 0)
                                {
                                    var v4 = ReadVertrex();
                                }

                                var left = ReadU32();
                                var top = ReadU32();
                                var right = ReadU32();
                                var bottom = ReadU32();

                                Application.Invoke(delegate
                                {
                                    store.AddNode(new MyTreeNode("RRIBC_RasterizeTriangle"), 0);
                                });
                            }
                            break;

                        default:
                            CloseSocket();
                            break;
                    }

                    old_cmd = command;
                }
            }
            catch
            {
                CloseSocket();
                return;
            }
        }
    }

    byte ReadU8()
    {
        byte[] data = new byte[sizeof(byte)];

        var bytes = 0;

        do
        {
            bytes += tcpClient.GetStream().Read(data, 0, data.Length);
        } while (bytes != data.Length);


        return data[0];
    }

    UInt32 ReadU32()
    {
        byte[] data = new byte[sizeof(UInt32)];

        var bytes = 0;

        do
        {
            bytes += tcpClient.GetStream().Read(data, 0, data.Length);
        } while (bytes != data.Length);

        return BitConverter.ToUInt32(data, 0);
    }

    float ReadF32()
    {
        byte[] data = new byte[sizeof(float)];

        var bytes = 0;

        do
        {
            bytes += tcpClient.GetStream().Read(data, 0, data.Length);
        } while (bytes != data.Length);

        return BitConverter.ToSingle(data, 0);
    }

    byte[] ReadBuffers()
    {
        byte[] data = new byte[32 * 32 * 4 * 6];

        var bytes = 0;

        do
        {
            bytes += tcpClient.GetStream().Read(data, 0, data.Length);
        } while (bytes != data.Length);

        return data;
    }

    RRI_DebugCommands ReadCommand()
    {
        var cmd = ReadU8();

        return (RRI_DebugCommands)cmd;
    }

    DrawParams ReadDrawParams()
    {
        DrawParams rv = new DrawParams();

        rv.isp = ReadU32();
        rv.tsp = ReadU32();
        rv.tcw = ReadU32();
        rv.tsp2 = ReadU32();
        rv.tcw2 = ReadU32();

        return rv;
    }

    Vertex ReadVertrex()
    {
        Vertex rv = new Vertex();

        rv.x = ReadF32();
        rv.y = ReadF32();
        rv.z = ReadF32();

        rv.col = ReadU32();
        rv.spc = ReadU32();
        rv.u = ReadF32();
        rv.v = ReadF32();

        rv.col1 = ReadU32();
        rv.spc1 = ReadU32();
        rv.u1 = ReadF32();
        rv.v1 = ReadF32();

        return rv;
    }

    void WriteU8(byte value)
    {
        byte[] data = new byte[] { value };

        tcpClient.Client.Send(data);
    }

    void WriteCommand(RRI_DebugCommands cmd)
    {
        WriteU8((byte)cmd);
    }

    byte[] GetBuffers()
    {
        WriteCommand(RRI_DebugCommands.RRIBC_GetBufferData);
        var reply = ReadCommand();
        // should be OK

        var rv = ReadBuffers();

        return rv;
    }

    NodeStore store;
    public MainWindow() : base(Gtk.WindowType.Toplevel)
    {
        Build();

        nodeview1.AppendColumn("Command", new Gtk.CellRendererText(), "text", 0);
        nodeview1.ShowAll();

        store = new NodeStore(typeof(MyTreeNode));
        nodeview1.NodeStore = store;

        notebook1.CurrentPage = 3;
    }

    protected void OnDeleteEvent(object sender, DeleteEventArgs a)
    {
        CloseSocket();

        Application.Quit();
        a.RetVal = true;
    }

    static Random rand = new Random();

    protected void NextCommand(object sender, EventArgs e)
    {
        foreach (var img in new Image[] { image3, image4, image5, image6, image7, image8 })
        {
            var pixels = new byte[4 * 64 * 64];


            for (var i = 0; i < pixels.Length; i++)
            {
                pixels[i] = (byte)rand.Next(255);
            }

            var pixbuf = new Gdk.Pixbuf(pixels, true, 8, 64, 64, 64 * 4);

            img.Pixbuf = pixbuf;
        }



        /*
        store.AddNode(new MyTreeNode("3x0 (96-127, 0-31)", "ClearBuffers"), 0);
        store.AddNode(new MyTreeNode("3x0 (96-127, 0-31)", "AddFpuEntry"), 0);
        store.AddNode(new MyTreeNode("3x0 (96-127, 0-31)", "RasterizeTriangle"), 0);
        */
    }

   


    protected void Connect(object sender, EventArgs e)
    {
        var host = txtHostname.Text.Split(':');

        tcpClient = new TcpClient(host[0], int.Parse(host[1]));
        client = new Thread(ClientThread);

        client.Start();
    }

    protected void Disconnect(object sender, EventArgs e)
    {
        CloseSocket();
    }

    protected void EnableCoreDebugging(object sender, EventArgs e)
    {
        lock (tcpClient)
        {
            WriteCommand(RRI_DebugCommands.RRIBC_SetStep);
            WriteU8(1); // sending
            WriteU8(1); // stepping

            hasOK = false;
        }

        for (; ; )
        {
            lock (tcpClient)
            {
                if (hasOK == true)
                {
                    break;
                }
            }
        }
    }

    protected void DisableCoreDebugging(object sender, EventArgs e)
    {
        lock (tcpClient)
        {
            WriteCommand(RRI_DebugCommands.RRIBC_SetStep);
            WriteU8(0); // sending
            WriteU8(0); // stepping

            hasOK = false;
        }

        for (; ; )
        {
            lock (tcpClient)
            {
                if (hasOK == true)
                {
                    break;
                }
            }
        }
    }

    protected void CoreOnStep(object sender, EventArgs e)
    {
        lock (tcpClient)
        {
            if (corePaused)
            {
                corePaused = false;
                WriteCommand(RRI_DebugCommands.RRIBC_OK);
            }
        }
    }
}
