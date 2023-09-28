package com.reicast.emulator;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.emu.NativeGLView;
import com.reicast.emulator.periph.VJoy;

import java.util.regex.Pattern;

public final class NativeGLActivity extends BaseGLActivity {

    private static ViewGroup mLayout;   // used for text input
    public String input_cont;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        requestWindowFeature(Window.FEATURE_NO_TITLE);

        super.onCreate(savedInstanceState);

        // Create the actual GL view
        mView = new NativeGLView(this);
        mLayout = new RelativeLayout(this);
        mLayout.addView(mView);

        setContentView(mLayout);
    }

    @Override
    protected void doPause() {
        ((NativeGLView)mView).pause();
    }

    @Override
    protected void doResume() {
        ((NativeGLView)mView).resume();
    }

    @Override
    public boolean isSurfaceReady() {
        return ((NativeGLView)mView).isSurfaceReady();
    }

    // Called from native code
    public void VJoyStartEditing() {
        vjoy_d_cached = VJoy.readCustomVjoyValues(getApplicationContext());
        JNIdc.show_osd();
        ((NativeGLView)mView).setEditVjoyMode(true);
    }
    // Called from native code
    public void VJoyResetEditing() {
        VJoy.resetCustomVjoyValues(getApplicationContext());
        ((NativeGLView)mView).readCustomVjoyValues();
        ((NativeGLView)mView).resetEditMode();

        // force re setting of jndidc.vjoy via layout
        // *WATCH* we are not in Java UI thread here

        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                mView.requestLayout();
            }
        });
    }
    // Called from native code
    public void VJoyStopEditing(boolean canceled) {
        if (canceled)
            ((NativeGLView)mView).restoreCustomVjoyValues(vjoy_d_cached);
        ((NativeGLView)mView).setEditVjoyMode(false);
    }

    public void RecreateView() {
        final Activity activity = this;
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                mLayout.removeAllViews();
                mView = new NativeGLView(activity);
                mLayout.addView(mView);
            }
        });
    }

    class ReadyState { public boolean value = false;}

    public int Msgbox(final String text, final int type) {

        final ReadyState ready = new ReadyState();

        final Activity activity = this;

        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                AlertDialog.Builder builder = new AlertDialog.Builder(activity);

                String msg = text;

                if ((type & 0x10) != 0) { /* MBX_ICONERROR */
                    msg += "\n" + R.string.msgbox_please_report;
                }

                builder.setMessage(msg).setPositiveButton(R.string.msgbox_okay, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                        synchronized (ready) {
                            ready.value = true;
                            ready.notify();
                        }
                    }
                });
                builder.create().show();
            }
        });

        try
        {
            synchronized (ready) {
                while (!ready.value)
                    ready.wait();
            }
        } catch(InterruptedException is) {

        }

        return 1;
    }

    public String showVirtualKeyboard(final String filename){
        final NativeGLActivity.ReadyState ready = new NativeGLActivity.ReadyState();

        final Activity activity = this;

        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                LayoutInflater inflater = activity.getLayoutInflater();
                View nView = inflater.inflate(R.layout.layout_input, null);
                AlertDialog.Builder dlgAlert = new AlertDialog.Builder(activity);
                dlgAlert.setView(nView);
                dlgAlert.setTitle("Rename");
                dlgAlert.setMessage("Enter new file name:");
                String filenameArray[] = filename.split("\\.");
                String extension = filenameArray[filenameArray.length - 1];
                final EditText user_field = nView.findViewById(R.id.edit_rename);
                user_field.setHint(filenameArray[0]);
                final TextView extension_info = nView.findViewById(R.id.text_ext);
                extension_info.setText("." + extension);

                dlgAlert.setPositiveButton(R.string.msgbox_okay, null);
                dlgAlert.setNegativeButton("Cancel", null);
                final AlertDialog alertDialog = dlgAlert.show();


                Button button = alertDialog.getButton(AlertDialog.BUTTON_POSITIVE);
                Button button_n = alertDialog.getButton(AlertDialog.BUTTON_NEGATIVE);
                button.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        Pattern regex = Pattern.compile("[$&+,:;=\\\\?@#|/'<>.^*()%-]");
                        if ((user_field.getText().toString().trim().length() > 0) && !(regex.matcher(user_field.getText().toString()).find())){
                            synchronized (ready) {
                                ready.value = true;
                                if (!user_field.getText().toString().isEmpty())
                                    input_cont = user_field.getText().toString() + extension_info.getText().toString();
                                ready.notify();
                                alertDialog.dismiss();
                            }
                        } else {
                            user_field.getText().clear();
                            user_field.setHint("Invalid Name");

                        }

                    }
                });
                button_n.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        synchronized (ready) {
                            ready.value = true;
                            input_cont = "..";
                            ready.notify();
                            alertDialog.dismiss();
                        }

                    }
                });

            }
        });

        try
        {
            synchronized (ready) {
                while (!ready.value)
                    ready.wait();
            }
        } catch(InterruptedException is) {

        }
            String temp_variable = input_cont;
            input_cont = null;
            return temp_variable;
    }

    @TargetApi(19)
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus && Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            View decorView = getWindow().getDecorView();
            decorView.setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            /* not using SYSTEM_UI_FLAG_LAYOUT_STABLE  as it seems to be buggy on Pixel3a ~ skmp*/
        }
    }
}
