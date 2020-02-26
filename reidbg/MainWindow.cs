using System;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using Gtk;
using Reidbg;

[Gtk.TreeNode(ListOnly = true)]
public class MyTreeNode : Gtk.TreeNode
{

    public DrawParams param;

    public Vertex v1;
    public Vertex v2;
    public Vertex v3;
    public bool hasV4;
    public Vertex v4;

    public uint tileLeft, tileTop, tileRight, tileBottom;

    public byte[] buffers;

    public MyTreeNode(string artist)
    {
        Artist = artist;
    }

    [Gtk.TreeNodeValue(Column = 0)]
    public string Artist;
}

public class NativeTexture
{
    public byte Type;
    public int Width;
    public int Height;
    public byte[] Bytes;
};

public partial class MainWindow : Gtk.Window
{
    bool hasOK;
    bool waitingTexture;
    bool corePaused;

    bool waitingTile;
    bool waitingFrame;


    TcpClient tcpClient;

    Thread client;

    Vertex[] vertices;
    uint tileLeft, tileTop, tileRight, tileBottom;

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
            waitingTexture = false;
            corePaused = false;

            waitingTile = false;
            waitingFrame = false;
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
                            if (waitingTexture)
                            {
                                NativeTexture texture = null;

                                var type = ReadU8();

                                if (type != 0)
                                {
                                    texture = new NativeTexture();
                                    texture.Type = type;
                                    texture.Width = (int)ReadU32();
                                    texture.Height = (int)ReadU32();

                                    texture.Bytes = ReadBytes(texture.Width * texture.Height * 4 * 4);
                                }

                                SetTexture(texture);
                                waitingTexture = false;
                            }
                            else
                            {
                                hasOK = true;
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_StepNotification:
                            corePaused = true;
                            Application.Invoke(delegate
                            {
                                nodeview1.ScrollToCell(TreePath.NewFirst(), null, false, 0, 0);
                            });
                            break;
                        case RRI_DebugCommands.RRIBC_TileNotification:
                            {
                                var tileX = ReadU32();
                                var tileY = ReadU32();

                                Application.Invoke(delegate
                                {
                                    var node = new MyTreeNode(String.Format(
                                        "Tile Start {0}x{1}",
                                        tileX,
                                        tileY
                                    ));

                                    node.tileLeft = tileX;
                                    node.tileTop = tileY;

                                    node.tileRight = tileX + 32;
                                    node.tileBottom = tileY + 32;

                                    store.AddNode(node, 0);
                                });

                                if (waitingTile)
                                {
                                    waitingTile = false;

                                    WriteCommand(RRI_DebugCommands.RRIBC_SetStep);
                                    WriteU8(1); // sending
                                    WriteU8(1); // stepping

                                    hasOK = false;
                                }
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_FrameNotification:
                            {
                                var frame = ReadU32();

                                Application.Invoke(delegate
                                {
                                    var node = new MyTreeNode(String.Format(
                                        "Frame Start {0}",
                                        frame
                                    ));
                                    store.AddNode(node, 0);
                                });

                                if (waitingFrame)
                                {
                                    waitingFrame = false;
                                    WriteCommand(RRI_DebugCommands.RRIBC_SetStep);
                                    WriteU8(1); // sending
                                    WriteU8(1); // stepping

                                    hasOK = false;
                                }
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_ClearBuffers:
                            {
                                var paramValue = ReadU32();
                                var depthValue = ReadF32();
                                var stencilValue = ReadU32();

                                var buffers = ReadBuffers();

                                Application.Invoke(delegate
                                {
                                    var node = new MyTreeNode(String.Format(
                                        "ClearBuffers({0}, {1}, {2})",
                                        paramValue,
                                        depthValue,
                                        stencilValue
                                    ));
                                    node.buffers = buffers;
                                    store.AddNode(node, 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_PeelBuffers:
                            {
                                var paramValue = ReadU32();
                                var depthValue = ReadF32();
                                var stencilValue = ReadU32();

                                var buffers = ReadBuffers();

                                Application.Invoke(delegate
                                {
                                    var node = new MyTreeNode(String.Format(
                                        "PeelBuffers({0}, {1}, {2})",
                                        paramValue,
                                        depthValue,
                                        stencilValue
                                    ));
                                    node.buffers = buffers;
                                    store.AddNode(node, 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_SummarizeStencilOr:
                            {
                                var buffers = ReadBuffers();
                                Application.Invoke(delegate
                                {
                                    var node = new MyTreeNode("SummarizeStencilOr");
                                    node.buffers = buffers;
                                    store.AddNode(node, 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_SummarizeStencilAnd:
                            {
                                var buffers = ReadBuffers();
                                Application.Invoke(delegate
                                {
                                    var node = new MyTreeNode("RRIBC_SummarizeStencilAnd");
                                    node.buffers = buffers;
                                    store.AddNode(node, 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_ClearPixelsDrawn:
                            Application.Invoke(delegate
                            {
                                var node = new MyTreeNode("ClearPixelsDrawn");
                                store.AddNode(node, 0);
                            });
                            break;
                        case RRI_DebugCommands.RRIBC_GetPixelsDrawn:
                            {
                                var rv = ReadU32();

                                Application.Invoke(delegate
                                {
                                    var node = new MyTreeNode("GetPixelsDrawn = " + rv);
                                    store.AddNode(node, 0);
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
                                    var node = new MyTreeNode("AddFpuEntry(rm: " + render_mode + ") =" + rv);

                                    node.param = param;
                                    node.v1 = v1;
                                    node.v2 = v2;
                                    node.v3 = v3;
                                    node.hasV4 = false;

                                    store.AddNode(node, 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_ClearFpuEntries:
                            Application.Invoke(delegate
                            {
                                var node = new MyTreeNode("ClearFpuEntries");
                                store.AddNode(node, 0);
                            });
                            break;
                        case RRI_DebugCommands.RRIBC_GetColorOutputBuffer:
                            {
                                var buffers = ReadBuffers();

                                Application.Invoke(delegate
                                {
                                    var node = new MyTreeNode("GetColorOutputBuffer");
                                    node.buffers = buffers;
                                    store.AddNode(node, 0);
                                });
                            }
                            break;
                        case RRI_DebugCommands.RRIBC_RenderParamTags:
                            {
                                var rm = ReadU32();
                                var tileX = ReadU32();
                                var tileY = ReadU32();
                                var buffers = ReadBuffers();

                                Application.Invoke(delegate
                                {
                                    var node = new MyTreeNode(String.Format("RenderParamTags(rm: {0}, x: {1}, y: {2})", rm, tileX, tileY));
                                    node.buffers = buffers;

                                    node.tileLeft = tileX;
                                    node.tileTop = tileY;

                                    node.tileRight = tileX + 32;
                                    node.tileBottom = tileY + 32;

                                    store.AddNode(node, 0);
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

                                Vertex v4 = new Vertex();

                                if (has_v4 != 0)
                                {
                                    v4 = ReadVertrex();
                                }

                                var left = ReadU32();
                                var top = ReadU32();
                                var right = ReadU32();
                                var bottom = ReadU32();

                                var buffers = ReadBuffers();

                                Application.Invoke(delegate
                                {
                                    var node = new MyTreeNode(String.Format("RasterizeTriangle(rm: {0}, tag: {1}, vo: {2} area: {3},{4} - {5}, {6})", render_mode, tag, vertex_offset, left, top, right, bottom));

                                    node.param = param;
                                    node.v1 = v1;
                                    node.v2 = v2;
                                    node.v3 = v3;
                                    node.hasV4 = has_v4 != 0;
                                    node.v4 = v4;
                                    node.buffers = buffers;

                                    node.tileLeft = left;
                                    node.tileTop = top;
                                    node.tileRight = right;
                                    node.tileBottom = bottom;
                                    store.AddNode(node, 0);
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

    byte[] ReadBytes(int size)
    {
        byte[] data = new byte[size];

        var bytes = 0;
        do
        {
            bytes += tcpClient.GetStream().Read(data, bytes, data.Length - bytes);
        } while (bytes != data.Length);

        return data;
    }

    byte ReadU8()
    {
        return ReadBytes(sizeof(byte))[0];
    }

    UInt32 ReadU32()
    {
        return BitConverter.ToUInt32(ReadBytes(sizeof(UInt32)), 0);
    }

    float ReadF32()
    {
        return BitConverter.ToSingle(ReadBytes(sizeof(float)), 0);
    }

    byte[] ReadBuffers()
    {
        return ReadBytes(32 * 32 * 4 * 6);
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

    void WriteBytes(byte[] data)
    {
        tcpClient.GetStream().Write(data, 0, data.Length);
    }

    void WriteU8(byte value)
    {
        WriteBytes(new byte[] { value });
    }

    void WriteU32(UInt32 value)
    {
        WriteBytes(BitConverter.GetBytes(value));
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

    void GetTexture(UInt32 tsp, UInt32 tcw)
    {
        lock (tcpClient)
        {
            waitingTexture = true;
            WriteCommand(RRI_DebugCommands.RRIBC_GetTextureData);
            WriteU32(tsp);
            WriteU32(tcw);
        }

        for (; ; )
        {
            lock (tcpClient)
            {
                if (!waitingTexture)
                    break;
            }
        }
    }


    NodeStore store;
    public MainWindow() : base(Gtk.WindowType.Toplevel)
    {
        Build();

        nodeview1.NodeSelection.Changed += new System.EventHandler(CoreCommandSelected);

        nodeview1.AppendColumn("Command", new Gtk.CellRendererText(), "text", 0);
        nodeview1.ShowAll();

        store = new NodeStore(typeof(MyTreeNode));
        nodeview1.NodeStore = store;


        notebook1.CurrentPage = 3;

    }


    static Random rand = new Random();

    protected void NextCommand(object sender, EventArgs e)
    {
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

    public ITreeNode SelectedNode(NodeView nv)
    {
        TreePath[] paths = nv.Selection.GetSelectedRows();
        int length = paths.Length;

        if (length > 0)
        {
            return store.GetNode(paths[0]);
        }
        else
        {
            return null;
        }
    }

    void SetTexture(NativeTexture texture)
    {
        if (texture != null)
        {
            var data = new byte[texture.Width * texture.Height * 4];

            for (var y = 0; y < texture.Height; y++)
            {
                for (var x = 0; x < texture.Width; x++)
                {
                    var offs_src = (x + y * texture.Width) * 4 * 4 + 12;
                    var offs_dst = (x + y * texture.Width) * 4;

                    data[offs_dst + 0] = texture.Bytes[offs_src + 0];
                    data[offs_dst + 1] = texture.Bytes[offs_src + 1];
                    data[offs_dst + 2] = texture.Bytes[offs_src + 2];
                    data[offs_dst + 3] = texture.Bytes[offs_src + 3];
                }
            }

            var pixbuf = new Gdk.Pixbuf(data, true, 8, texture.Width, texture.Height, texture.Width * 4);

            imgTcw.Pixbuf = pixbuf;
        }
        else
        {
            imgTcw.Pixbuf = null;
        }
    }

    protected void CoreCommandSelected(object o, EventArgs e)
    {

        NodeSelection selection = (NodeSelection)o;

        if (SelectedNode(selection.NodeView) is MyTreeNode node)
        {
            if (node.buffers != null)
            {
                var images = new Image[] { image3, image4, image5, image6, image7, image8 };


                for (var i = 0; i < images.Length; i++)
                {
                    var pixels = new byte[4 * 32 * 32];


                    for (var j = 0; j < pixels.Length; j+=4)
                    {
                        pixels[j + 3] = node.buffers[i * 32 * 32 * 4 + j + 0];
                        pixels[j + 0] = node.buffers[i * 32 * 32 * 4 + j + 1];
                        pixels[j + 1] = node.buffers[i * 32 * 32 * 4 + j + 2];
                        pixels[j + 2] = node.buffers[i * 32 * 32 * 4 + j + 3];

                    }

                    var pixbuf = new Gdk.Pixbuf(pixels, true, 8, 32, 32, 32 * 4);

                    images[i].Pixbuf = pixbuf;
                }
            }

            StringBuilder sb = new StringBuilder();

            if (node.hasV4)
            {
                vertices = new Vertex[] { node.v1, node.v2, node.v3, node.v4 };
            }
            else
            {
                vertices = new Vertex[] { node.v1, node.v2, node.v3 };
            }

            tileLeft = node.tileLeft;
            tileTop = node.tileTop;
            tileRight = node.tileRight;
            tileBottom = node.tileBottom;

            foreach (var vertex in vertices)
            {
                sb.AppendLine("vtx {");
                sb.AppendLine(String.Format("x: {0} y: {1} z: {2}", vertex.x, vertex.y, vertex.z));
                sb.AppendLine(String.Format("u: {0} v: {1}", vertex.u, vertex.v));
                sb.AppendLine(String.Format("u1: {0} v1: {1}", vertex.u1, vertex.v1));
                sb.AppendLine(String.Format("col: {0} spc: {1}", Convert.ToString(vertex.col,16), Convert.ToString(vertex.spc, 16)));
                sb.AppendLine(String.Format("col1: {0} spc1: {1}", Convert.ToString(vertex.col1, 16), Convert.ToString(vertex.spc1, 16)));
                sb.AppendLine("}");
            }

            txtVertexes.Buffer.Text = sb.ToString();
            daVertexes.QueueDraw();

            sb = new StringBuilder();
            sb.AppendLine(String.Format("isp/tsp: {0}", Convert.ToString(node.param.isp, 16)));
            sb.AppendLine(String.Format("tsp: {0}", Convert.ToString(node.param.tsp, 16)));
            sb.AppendLine(String.Format("tsp1: {0}", Convert.ToString(node.param.tsp2, 16)));
            sb.AppendLine(String.Format("tcw: {0}", Convert.ToString(node.param.tcw, 16)));
            sb.AppendLine(String.Format("tcw1: {0}", Convert.ToString(node.param.tcw2, 16)));

            txtIspTsp.Buffer.Text = sb.ToString();

            sb = new StringBuilder();
            sb.AppendLine(String.Format("tcw: {0}", Convert.ToString(node.param.tcw, 16)));
            sb.AppendLine(String.Format("tcw1: {0}", Convert.ToString(node.param.tcw2, 16)));

            txtTcw.Buffer.Text = sb.ToString();

            GetTexture(node.param.tsp, node.param.tcw);
        }



    }

    protected void daVertexes_OnDraw(object sender, ExposeEventArgs args)
    {
        if (vertices != null)
        {
            DrawingArea area = (DrawingArea)sender;
            Cairo.Context cr = Gdk.CairoHelper.Create(area.GdkWindow);

            float minX = 0;
            float minY = 0;
            float maxX = 640;
            float maxY = 480;

            foreach (var vtx in vertices)
            {
                minX = Math.Min(minX, vtx.x);
                minY = Math.Min(minY, vtx.y);

                maxX = Math.Max(maxX, vtx.x);
                maxY = Math.Max(maxY, vtx.y);
            }

            minX -= 32;
            minY -= 32;

            maxX += 32;
            maxY += 32;

            float width = area.Allocation.Width;
            float height = area.Allocation.Height;

            cr.Scale(width / (maxX - minX), height /  (maxY - minY));
            cr.Translate(-minX, -minY);

            cr.LineWidth = 1;

            cr.Rectangle(0, 0, 640, 480);
            cr.StrokePreserve();
            cr.SetSourceRGB(1, 1, 1);
            cr.Fill();

            cr.Rectangle(tileLeft, tileTop, tileRight- tileLeft, tileBottom - tileTop);
            cr.StrokePreserve();
            cr.SetSourceRGB(0.9, 0.9, 0.9);
            cr.Fill();

            cr.MoveTo(vertices[0].x, vertices[0].y);
            foreach(var vtx in vertices)
            {
                cr.LineTo(vtx.x, vtx.y);
            }

            cr.ClosePath();

            cr.StrokePreserve();

            cr.SetSourceRGB(0.3, 0.4, 0.6);
            cr.Fill();

            ((IDisposable)cr.GetTarget()).Dispose();
            ((IDisposable)cr).Dispose();
        }
    }

    ~MainWindow()
    {
        CloseSocket();
    }

    protected void Window_OnDelete(object o, DeleteEventArgs args)
    {
        CloseSocket();

        Application.Quit();
        args.RetVal = true;
    }

    protected void CoreNextFrame(object sender, EventArgs e)
    {
        waitingFrame = true;

        lock (tcpClient)
        {
            WriteCommand(RRI_DebugCommands.RRIBC_SetStep);
            WriteU8(1); // sending
            WriteU8(0); // stepping

            hasOK = false;

            if (corePaused)
            {
                corePaused = false;
                WriteCommand(RRI_DebugCommands.RRIBC_OK);
            }
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

    protected void CoreNextTile(object sender, EventArgs e)
    {
        waitingTile = true;

        lock (tcpClient)
        {
            WriteCommand(RRI_DebugCommands.RRIBC_SetStep);
            WriteU8(1); // sending
            WriteU8(0); // stepping

            hasOK = false;

            if (corePaused)
            {
                corePaused = false;
                WriteCommand(RRI_DebugCommands.RRIBC_OK);
            }
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

    protected void CoreClearList(object sender, EventArgs e)
    {
        store.Clear();
    }
}
