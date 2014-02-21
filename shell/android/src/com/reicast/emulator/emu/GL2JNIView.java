package com.reicast.emulator.emu;


import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Vibrator;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.ScaleGestureDetector.SimpleOnScaleGestureListener;
import android.view.View;

import com.reicast.emulator.GL2JNIActivity;
import com.reicast.emulator.config.Config;
import com.reicast.emulator.emu.OnScreenMenu.FpsPopup;
import com.reicast.emulator.periph.VJoy;


/**
 * A simple GLSurfaceView sub-class that demonstrate how to perform
 * OpenGL ES 2.0 rendering into a GL Surface. Note the following important
 * details:
 *
 * - The class must use a custom context factory to enable 2.0 rendering.
 *   See ContextFactory class definition below.
 *
 * - The class must use a custom EGLConfigChooser to be able to select
 *   an EGLConfig that supports 2.0. This is done by providing a config
 *   specification to eglChooseConfig() that has the attribute
 *   EGL10.ELG_RENDERABLE_TYPE containing the EGL_OPENGL_ES2_BIT flag
 *   set. See ConfigChooser class definition below.
 *
 * - The class must select the surface's format, then choose an EGLConfig
 *   that matches it exactly (with regards to red/green/blue/alpha channels
 *   bit depths). Failure to do so would result in an EGL_BAD_MATCH error.
 */

public class GL2JNIView extends GLSurfaceView
{
  private static String fileName;
  //private AudioThread audioThread;  
  private EmuThread ethd;
  
  Vibrator vib;

  private boolean editVjoyMode = false;
  private int selectedVjoyElement = -1;
  private ScaleGestureDetector scaleGestureDetector;
  
  public float[][] vjoy_d_custom;

  private static final float[][] vjoy = VJoy.baseVJoy();

  Renderer rend;

  private boolean touchVibrationEnabled;
  Context context;
  
  public void restoreCustomVjoyValues(float[][] vjoy_d_cached) {
	  vjoy_d_custom = vjoy_d_cached;
	  VJoy.writeCustomVjoyValues(vjoy_d_cached, context);

      resetEditMode();
      requestLayout();
  }

  public void setFpsDisplay(FpsPopup fpsPop) {
	  rend.fpsPop = fpsPop;
  }
  	
	public GL2JNIView(Context context, Config config, String newFileName,
			boolean translucent, int depth, int stencil, boolean editVjoyMode) {
    super(context);
    this.context = context;
    this.editVjoyMode = editVjoyMode;
    setKeepScreenOn(true);
    
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
    setOnSystemUiVisibilityChangeListener (new OnSystemUiVisibilityChangeListener() {
	      public void onSystemUiVisibilityChange(int visibility) {
	        if ((visibility & SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
	          GL2JNIView.this.setSystemUiVisibility(
	            SYSTEM_UI_FLAG_IMMERSIVE_STICKY
	            | SYSTEM_UI_FLAG_FULLSCREEN
	            | SYSTEM_UI_FLAG_HIDE_NAVIGATION);
	        }
	      }
	    });
    }

    vib=(Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
    
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
    	Runtime.getRuntime().freeMemory();
    	System.gc();
    }

    SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
    boolean soundEndabled = prefs.getBoolean("sound_enabled", true);
    ethd = new EmuThread(soundEndabled);
    if (!soundEndabled) {
    	Thread.currentThread().setPriority(Thread.MAX_PRIORITY);
    	// Ensures priority is not placed on disabled sound thread
    }
    
    touchVibrationEnabled = prefs.getBoolean("touch_vibration_enabled", true);
    
    int renderType = prefs.getInt("render_type", LAYER_TYPE_HARDWARE);
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
    	this.setLayerType(renderType, null);
    } else {
    	try {
    		Method setLayerType = this.getClass().getMethod(
    				"setLayerType", new Class[] { int.class, Paint.class });
    		if (setLayerType != null)
    			setLayerType.invoke(this, new Object[] { renderType, null });
    	} catch (NoSuchMethodException e) {
    	} catch (IllegalArgumentException e) {
    	} catch (IllegalAccessException e) {
    	} catch (InvocationTargetException e) {
    	}
    }

    config.loadConfigurationPrefs();
    
    vjoy_d_custom = VJoy.readCustomVjoyValues(context);

    scaleGestureDetector = new ScaleGestureDetector(context, new OscOnScaleGestureListener());

    // This is the game we are going to run
    fileName = newFileName;

    if (GL2JNIActivity.syms != null)
    	JNIdc.data(1, GL2JNIActivity.syms);

//    int[] kcode = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
//    int[] rt = { 0, 0, 0, 0 }, lt = { 0, 0, 0, 0 };
//    int[] jx = { 128, 128, 128, 128 }, jy = { 128, 128, 128, 128 };
    JNIdc.init(fileName);

    // By default, GLSurfaceView() creates a RGB_565 opaque surface.
    // If we want a translucent one, we should change the surface's
    // format here, using PixelFormat.TRANSLUCENT for GL Surfaces
    // is interpreted as any 32-bit surface with alpha by SurfaceFlinger.
    if(translucent) this.getHolder().setFormat(PixelFormat.TRANSLUCENT);
    
    if (prefs.getBoolean("force_gpu", false)) {
    	setEGLContextFactory(new GLCFactory6.ContextFactory());
    	setEGLConfigChooser(
    			translucent?
    					new GLCFactory6.ConfigChooser(8, 8, 8, 8, depth, stencil)
    			: new GLCFactory6.ConfigChooser(5, 6, 5, 0, depth, stencil)
    			);
    } else {
    	// Setup the context factory for 2.0 rendering.
    	// See ContextFactory class definition below
    	setEGLContextFactory(new GLCFactory.ContextFactory());

    	// We need to choose an EGLConfig that matches the format of
    	// our surface exactly. This is going to be done in our
    	// custom config chooser. See ConfigChooser class definition
    	// below.
    	setEGLConfigChooser(
    			translucent?
    					new GLCFactory.ConfigChooser(8, 8, 8, 8, depth, stencil)
    			: new GLCFactory.ConfigChooser(5, 6, 5, 0, depth, stencil)
    			);
    }

    // Set the renderer responsible for frame rendering
    setRenderer(rend=new Renderer(this));

    // Initialize audio
    //configAudio(44100,250);
   
    ethd.start();
  }
  
  public GLSurfaceView.Renderer getRenderer()
  {
	  return rend;
  }

  private static void LOGI(String S) { Log.i("GL2JNIView",S); }
  private static void LOGW(String S) { Log.w("GL2JNIView",S); }
  private static void LOGE(String S) { Log.e("GL2JNIView",S); }

//  public void configAudio(int rate,int latency)
//  {
//    //if(audioThread!=null) audioThread.stopPlayback();
//    //audioThread = new AudioThread(rate,latency);
//  }

  private void reset_analog()
  {
	  
    int j=11;
    vjoy[j+1][0]=vjoy[j][0]+vjoy[j][2]/2-vjoy[j+1][2]/2;
    vjoy[j+1][1]=vjoy[j][1]+vjoy[j][3]/2-vjoy[j+1][3]/2;
    JNIdc.vjoy(j+1, vjoy[j+1][0], vjoy[j+1][1], vjoy[j+1][2], vjoy[j+1][3]);
  }
  
  int get_anal(int j, int axis)
  {
	  return (int) (((vjoy[j+1][axis]+vjoy[j+1][axis+2]/2) - vjoy[j][axis] - vjoy[j][axis+2]/2)*254/vjoy[j][axis+2]);
  }
  
  float vbase(float p, float m, float scl)
  {
	  return (int) ( m - (m -p)*scl);
  }
  
  float vbase(float p, float scl)
  {
	  return (int) (p*scl );
  }
  
  public boolean isTablet() {
    return (getContext().getResources().getConfiguration().screenLayout
            & Configuration.SCREENLAYOUT_SIZE_MASK)
            >= Configuration.SCREENLAYOUT_SIZE_LARGE;
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) 
  {  
		super.onLayout(changed, left, top, right, bottom);
		//dcpx/cm = dcpx/px * px/cm
                float magic = isTablet() ? 0.8f : 0.7f;
		float scl=480.0f/getHeight() * getContext().getResources().getDisplayMetrics().density * magic;
		float scl_dc=getHeight()/480.0f;
		float tx  = ((getWidth()-640.0f*scl_dc)/2)/scl_dc;
		
		float a_x = -tx+ 24*scl;
		float a_y=- 24*scl;
		
                float[][] vjoy_d = VJoy.getVjoy_d(vjoy_d_custom);

		for(int i=0;i<vjoy.length;i++)
		{
			if (vjoy_d[i][0] == 288)
				vjoy[i][0] = vjoy_d[i][0];
			else if (vjoy_d[i][0]-vjoy_d_custom[getElementIdFromButtonId(i)][0] < 320)
				vjoy[i][0] = a_x + vbase(vjoy_d[i][0],scl);
			else
				vjoy[i][0] = -a_x + vbase(vjoy_d[i][0],640,scl);
			
			vjoy[i][1] = a_y + vbase(vjoy_d[i][1],480,scl);
			
			vjoy[i][2] = vbase(vjoy_d[i][2],scl);
			vjoy[i][3] = vbase(vjoy_d[i][3],scl);
		}
		
		for(int i=0;i<vjoy.length;i++)
		      JNIdc.vjoy(i,vjoy[i][0],vjoy[i][1],vjoy[i][2],vjoy[i][3]);
		    
		reset_analog();
		VJoy.writeCustomVjoyValues(vjoy_d_custom, context);
	}
  
  /*
   * 
   * 	DOWN / POINTER_DOWN
   * 	UP / CANCEL -> reset state
   * 	POINTER_UP -> check for freed analog
   * */
  int anal_id=-1, lt_id=-1, rt_id=-1;
  /*
  bool intersects(CircleType circle, RectType rect)
  {
      circleDistance.x = abs(circle.x - rect.x);
      circleDistance.y = abs(circle.y - rect.y);

      if (circleDistance.x > (rect.width/2 + circle.r)) { return false; }
      if (circleDistance.y > (rect.height/2 + circle.r)) { return false; }

      if (circleDistance.x <= (rect.width/2)) { return true; } 
      if (circleDistance.y <= (rect.height/2)) { return true; }

      cornerDistance_sq = (circleDistance.x - rect.width/2)^2 +
                           (circleDistance.y - rect.height/2)^2;

      return (cornerDistance_sq <= (circle.r^2));
  }
  */

  public void resetEditMode() {
        editLastX = 0;
        editLastY = 0;
  }

  private static int getElementIdFromButtonId(int buttonId) {
       if (buttonId <= 3)
            return 0; // DPAD
       else if (buttonId <= 7)
            return 1; // X, Y, B, A Buttons
       else if (buttonId == 8)
            return 2; // Start
       else if (buttonId == 9)
            return 3; // Left Trigger
       else if (buttonId == 10)
            return 4; // Right Trigger
       else if (buttonId <= 12)
            return 5; // Analog
       else
            return -1; // Invalid
  }

  public static int[] kcode_raw = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
  public static int[] lt = new int[4];
  public static int[] rt = new int[4];
  public static int[] jx = new int[4];
  public static int[] jy = new int[4];

  float editLastX = 0, editLastY = 0;

  @Override public boolean onTouchEvent(final MotionEvent event) 
  {
  JNIdc.show_osd();

  scaleGestureDetector.onTouchEvent(event);
  
  float ty  = 0.0f;
  float scl  = getHeight()/480.0f;
  float tx  = (getWidth()-640.0f*scl)/2;

  int   rv  = 0xFFFF;
  
  int   aid = event.getActionMasked();
  int   pid = event.getActionIndex();

  if (editVjoyMode && selectedVjoyElement != -1 && aid == MotionEvent.ACTION_MOVE && !scaleGestureDetector.isInProgress()) {
       float x = (event.getX()-tx)/scl;
       float y = (event.getY()-ty)/scl;

       if (editLastX != 0 && editLastY != 0) {
            float deltaX = x - editLastX;
            float deltaY = y - editLastY;

            vjoy_d_custom[selectedVjoyElement][0] += isTablet() ? deltaX * 2 : deltaX;
            vjoy_d_custom[selectedVjoyElement][1] += isTablet() ? deltaY * 2 : deltaY;

            requestLayout();
       }

       editLastX = x;
       editLastY = y;

       return true;
  }
  
  //LOGI("Touch: " + aid + ", " + pid);
    
  for(int i=0;i<event.getPointerCount();i++)
  { 
	float x = (event.getX(i)-tx)/scl;
	float y = (event.getY(i)-ty)/scl;
	if (anal_id!=event.getPointerId(i))
	{
		if (aid==MotionEvent.ACTION_POINTER_UP && pid==i)
			continue;
		for(int j=0;j<vjoy.length;j++)
		{
		  if(x>vjoy[j][0] && x<=(vjoy[j][0]+vjoy[j][2]))
		  {
			int pre=(int)(event.getPressure(i)*255);
			if (pre>20)
			{
				pre-=20;
				pre*=7;
			}
			if (pre>255) pre=255;
			
		    if(y>vjoy[j][1] && y<=(vjoy[j][1]+vjoy[j][3]))
		    {
		    	if (vjoy[j][4]>=-2)
		    	{
		    		if (vjoy[j][5]==0)
					if (!editVjoyMode && touchVibrationEnabled)
			    			vib.vibrate(50);
		    		vjoy[j][5]=2;
		    	}
		    	
		      
		      if(vjoy[j][4]==-3)
		      {
                          if (editVjoyMode) {
                                selectedVjoyElement = 5; // Analog
                                resetEditMode();
                          } else {
        		        vjoy[j+1][0]=x-vjoy[j+1][2]/2;
        		        vjoy[j+1][1]=y-vjoy[j+1][3]/2;
        		  
        		        JNIdc.vjoy(j+1, vjoy[j+1][0], vjoy[j+1][1] , vjoy[j+1][2], vjoy[j+1][3]);
        		        anal_id=event.getPointerId(i);
                          }
	          }
		  else if (vjoy[j][4]==-4);
	          else if(vjoy[j][4]==-1) {
                          if (editVjoyMode) {
                                selectedVjoyElement = 3; // Left Trigger
                                resetEditMode();
                          } else {
                                lt[0]=pre;
                                lt_id=event.getPointerId(i);
                          }
                  }
	          else if(vjoy[j][4]==-2) {
                          if (editVjoyMode) {
                                selectedVjoyElement = 4; // Right Trigger
                                resetEditMode();
                          } else{
                                rt[0]=pre;
                                rt_id=event.getPointerId(i);
                          }
                  }
	          else {
                          if (editVjoyMode) {
                                selectedVjoyElement = getElementIdFromButtonId(j);
                                resetEditMode();
                          } else
	        	        rv&=~(int)vjoy[j][4];
                  }
	        }
		  }
		}
	  }
	  else
	  {
		  if (x<vjoy[11][0])
			  x=vjoy[11][0];
		  else if (x>(vjoy[11][0]+vjoy[11][2]))
			  x=vjoy[11][0]+vjoy[11][2];
		  
		  if (y<vjoy[11][1])
			  y=vjoy[11][1];
		  else if (y>(vjoy[11][1]+vjoy[11][3]))
			  y=vjoy[11][1]+vjoy[11][3];
		  
		  int j=11;
		  vjoy[j+1][0]=x-vjoy[j+1][2]/2;
		  vjoy[j+1][1]=y-vjoy[j+1][3]/2;
		  
		  JNIdc.vjoy(j+1, vjoy[j+1][0], vjoy[j+1][1] , vjoy[j+1][2], vjoy[j+1][3]);
		  
	  }
  }
  
  for(int j=0;j<vjoy.length;j++)
	{
		if (vjoy[j][5]==2)
			vjoy[j][5]=1;
		else if (vjoy[j][5]==1)
			vjoy[j][5]=0;
	}
  
  switch(aid)
  {
  	case MotionEvent.ACTION_UP:
  	case MotionEvent.ACTION_CANCEL:
  		selectedVjoyElement = -1;
  		reset_analog();
  		anal_id=-1;
  		rv=0xFFFF;
  		rt[0]=0;
  		lt[0]=0;
  		lt_id=-1;
  		rt_id=-1;
  		for(int j=0;j<vjoy.length;j++)
  			vjoy[j][5]=0;
	break;
	
  	case MotionEvent.ACTION_POINTER_UP:
  		if (event.getPointerId(event.getActionIndex())==anal_id)
  		{
  			reset_analog();
  			anal_id=-1;
  		}
                else if (event.getPointerId(event.getActionIndex())==lt_id)
                {
                        lt[0]=0;
  			lt_id=-1;
                }
                else if (event.getPointerId(event.getActionIndex())==rt_id)
                {
                        rt[0]=0;
  			rt_id=-1;
                }
	break;
	
  	case MotionEvent.ACTION_POINTER_DOWN:
  	case MotionEvent.ACTION_DOWN:
	break;
  }

    /*
    if(GL2JNIActivity.keys[3]!=0) rv&=~key_CONT_DPAD_RIGHT;
    if(GL2JNIActivity.keys[2]!=0) rv&=~key_CONT_DPAD_LEFT;
    if(GL2JNIActivity.keys[1]!=0) rv&=~key_CONT_A;
    if(GL2JNIActivity.keys[0]!=0) rv&=~key_CONT_B;
    */
	  
    kcode_raw[0] = rv;
    jx[0] = get_anal(11, 0);
    jy[0] = get_anal(11, 1);
    pushInput();
    return(true);
  }

private class OscOnScaleGestureListener extends
  SimpleOnScaleGestureListener {

 @Override
 public boolean onScale(ScaleGestureDetector detector) {
  if (editVjoyMode && selectedVjoyElement != -1) {
       vjoy_d_custom[selectedVjoyElement][2] *= detector.getScaleFactor();
       requestLayout();

       return true;
  }

  return false;
 }

 @Override
 public void onScaleEnd(ScaleGestureDetector detector) {
  selectedVjoyElement = -1;
 }
}
  
  public void pushInput(){
	  JNIdc.kcode(kcode_raw,lt,rt,jx,jy);
  }

  private static class Renderer implements GLSurfaceView.Renderer
  {

	  private GL2JNIView mView;
	  private FPSCounter fps = new FPSCounter();
	  private FpsPopup fpsPop;

	  public Renderer (GL2JNIView mView) {
		  this.mView = mView;
	  }

	  public void onDrawFrame(GL10 gl)
	  {
		  if (fpsPop != null && fpsPop.isShowing()) {
			  fps.logFrame();
		  }
		  JNIdc.rendframe();
	  }

	  public void onSurfaceChanged(GL10 gl,int width,int height)
	  {
		  JNIdc.rendinit(width,height);
	  }

	  public void onSurfaceCreated(GL10 gl,EGLConfig config)
	  {
		  onSurfaceChanged(gl, 800, 480);
	  }

	  public class FPSCounter {
		  long startTime = System.nanoTime();
		  int frames = 0;

		  public void logFrame() {
			  frames++;
			  if (System.nanoTime() - startTime >= 1000000000) {
				  mView.post(new Runnable() {
					  public void run() {
						  if (frames > 0) {
							  fpsPop.setText(frames);
						  }
					  }
				  });
				  startTime = System.nanoTime();
				  frames = 0;
			  }
		  }
	  }
  }
  
  public void audioDisable(boolean disabled) {
	  if (disabled) {
		  ethd.Player.pause();
	  } else {
		  ethd.Player.play();
	  }
  }

  class EmuThread extends Thread
  {
	AudioTrack Player;
	long pos;	//write position
	long size;	//size in frames
	private boolean sound;
	
	public EmuThread(boolean sound) {
		this.sound = sound;
	}
	
    @Override public void run()
    {
    	if (sound) {
    	int min=AudioTrack.getMinBufferSize(44100,AudioFormat.CHANNEL_OUT_STEREO,AudioFormat.ENCODING_PCM_16BIT);
    	
    	if (2048>min)
    		min=2048;
    	
    	Player = new AudioTrack(
    	        AudioManager.STREAM_MUSIC,
    	        44100,
    	        AudioFormat.CHANNEL_OUT_STEREO,
    	        AudioFormat.ENCODING_PCM_16BIT,
    	        min,
    	        AudioTrack.MODE_STREAM
    	      );
    	
    	size=min/4; 
    	pos=0;
    	
    	Log.i("audcfg", "Audio streaming: buffer size " + min + " samples / " + min/44100.0 + " ms");
    	Player.play();
    	}
    	 
    	JNIdc.run(this);
    }
    
    int WriteBuffer(short[] samples, int wait)
    {
    	if (sound) {
    	int newdata=samples.length/2;
    	
    	if (wait==0)
    	{
    		//user bytes = write-read
    		//available = size - (write - play)
    		long used=pos-Player.getPlaybackHeadPosition();
    		long avail=size-used;
    		
    		//Log.i("AUD", "u: " + used + " a: " + avail);
    		if (avail<newdata)
    			return 0;
    	}
    	
    	pos+=newdata;
    	
    	Player.write(samples, 0, samples.length);
    	}
    	
    	return 1;
    }
  }
  
  //
  // Thread responsible for playing audio.
  //
  /*
  class AudioThready extends Thread
  {
    private AudioTrack Player;
    private int Rate;
    private int Latency;
    private int Chunk;
    private short Data[];

    public AudioThready(int AudioRate,int AudioLatency)
    {
      Rate    = (AudioRate==0)||(AudioRate>=8192)? AudioRate:8192;
      Latency = AudioLatency>=50? AudioLatency:50;
      Chunk   = 2048;
      Data    = new short[Chunk*2];
      start();
    }

    public void stopPlayback()
    { Rate=0; }

    public void pausePlayback(boolean Switch)
    {
      // Must have a valid player
      if((Player==null) || (Player.getState()!=AudioTrack.STATE_INITIALIZED)) return;

      // Switch between playback and pause
      if(Switch) { if(Player.getPlayState()==AudioTrack.PLAYSTATE_PLAYING) Player.pause(); }
      else       { if(Player.getPlayState()!=AudioTrack.PLAYSTATE_PLAYING) Player.play(); }
    }

    @Override public void run()
    {
      int Size,Min;

      LOGI("Starting audio thread for Rate="+Rate+"Hz, Latency="+Latency+"ms");

      // When no audio sampling rate supplied, do not play
      if(Rate<=0) return;

      // Compute minimal and requested buffer sizes
      Min  = AudioTrack.getMinBufferSize(Rate,AudioFormat.CHANNEL_OUT_STEREO,AudioFormat.ENCODING_PCM_16BIT);
      Size = 2*2*Chunk*2;

      // Create audio player
      Player = new AudioTrack(
        AudioManager.STREAM_MUSIC,
        Rate,
        AudioFormat.CHANNEL_OUT_STEREO,
        AudioFormat.ENCODING_PCM_16BIT,
        Min>Size? Min:Size,
        AudioTrack.MODE_STREAM
      );

      // Start playback
      Player.play();
 
      // Continue writing data, until requested to quit
      while(Rate>0)
      {
        //Size = JNIdc.play(Data,Chunk);
        if(Size>0) Player.write(Data,0,2*Size); else yield();
      }

      // Stop playback
      Player.stop();
      Player.flush();
      Player.release();

      LOGI("Exiting audio thread");
    }
  }
	*/
  
  public void onStop() {
	  // TODO Auto-generated method stub
	  System.exit(0);
	  try {
		  ethd.join();
	  } catch (InterruptedException e) {
		  // TODO Auto-generated catch block
		  e.printStackTrace();
	  }
  }
  
  @Override
  public void onWindowFocusChanged(boolean hasFocus) {
          super.onWindowFocusChanged(hasFocus);
      if (hasFocus && Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
          GL2JNIView.this.setSystemUiVisibility(
                  View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                  | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                  | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                  | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                  | View.SYSTEM_UI_FLAG_FULLSCREEN
                  | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);}
  }
}
