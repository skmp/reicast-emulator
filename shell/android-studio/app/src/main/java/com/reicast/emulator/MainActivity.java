package com.reicast.emulator;

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.design.widget.NavigationView;
import android.support.v4.app.Fragment;
import android.support.v4.view.GravityCompat;
import android.support.v4.widget.DrawerLayout;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnSystemUiVisibilityChangeListener;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import com.reicast.emulator.config.Config;
import com.reicast.emulator.config.InputFragment;
import com.reicast.emulator.config.OptionsFragment;
import com.reicast.emulator.debug.GenerateLogs;
import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.periph.Gamepad;

import java.io.File;
import java.lang.Thread.UncaughtExceptionHandler;
import java.util.List;

public class MainActivity extends AppCompatActivity implements
		FileBrowser.OnItemSelectedListener, OptionsFragment.OnClickListener,
		NavigationView.OnNavigationItemSelectedListener {

	private SharedPreferences mPrefs;
	private static File sdcard = Environment.getExternalStorageDirectory();
	public static String home_directory = sdcard.getAbsolutePath();

	private TextView menuHeading;
	private boolean hasAndroidMarket = false;
	
	private UncaughtExceptionHandler mUEHandler;

	Gamepad pad = new Gamepad();

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
			getWindow().getDecorView().setOnSystemUiVisibilityChangeListener (new OnSystemUiVisibilityChangeListener() {
				public void onSystemUiVisibilityChange(int visibility) {
					if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
						getWindow().getDecorView().setSystemUiVisibility(
//                            View.SYSTEM_UI_FLAG_LAYOUT_STABLE | 
								View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_FULLSCREEN
                                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
					}
				}
			});
		} else {
			getWindow().setFlags (WindowManager.LayoutParams.FLAG_FULLSCREEN,
                    WindowManager.LayoutParams.FLAG_FULLSCREEN);
		}

		mPrefs = PreferenceManager.getDefaultSharedPreferences(this);

		String prior_error = mPrefs.getString("prior_error", null);
		if (prior_error != null) {
			displayLogOutput(prior_error);
			mPrefs.edit().remove("prior_error").commit();
		} else {
			mUEHandler = new Thread.UncaughtExceptionHandler() {
				public void uncaughtException(Thread t, Throwable error) {
					if (error != null) {
						StringBuilder output = new StringBuilder();
						for (StackTraceElement trace : error.getStackTrace()) {
							output.append(trace.toString() + "\n");
						}
						mPrefs.edit().putString("prior_error", output.toString()).commit();
						error.printStackTrace();
						android.os.Process.killProcess(android.os.Process.myPid());
						System.exit(0);
					}
				}
			};
			Thread.setDefaultUncaughtExceptionHandler(mUEHandler);
		}

		home_directory = mPrefs.getString("home_directory", home_directory);

		Intent market = new Intent(Intent.ACTION_VIEW, Uri.parse("market://search?q=dummy"));
		if (isCallable(market)) {
			hasAndroidMarket = true;
		}
		
		if (!getFilesDir().exists()) {
			getFilesDir().mkdir();
		}
		JNIdc.config(home_directory);
		
		// When viewing a resource, pass its URI to the native code for opening
		Intent intent = getIntent();
		if (intent.getAction() != null) {
			if (intent.getAction().equals(Intent.ACTION_VIEW)) {
				onGameSelected(Uri.parse(intent.getData().toString()));
				// Flush the intent to prevent multiple calls
				getIntent().setData(null);
		        setIntent(null);
		        Config.externalIntent = true;
			}
		}

		// Check that the activity is using the layout version with
		// the fragment_container FrameLayout
		if (findViewById(R.id.fragment_container) != null) {

			// However, if we're being restored from a previous state,
			// then we don't need to do anything and should return or else
			// we could end up with overlapping fragments.
			if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB_MR1) {
				if (savedInstanceState != null) {
					return;
				}
			}

			// Create a new Fragment to be placed in the activity layout
			FileBrowser firstFragment = new FileBrowser();
			Bundle args = new Bundle();
			args.putBoolean("ImgBrowse", true);
			args.putString("browse_entry", null);
			// specify a path for selecting folder options
			args.putBoolean("games_entry", false);
			// specify if the desired path is for games or data
			firstFragment.setArguments(args);
			// In case this activity was started with special instructions from
			// an
			// Intent, pass the Intent's extras to the fragment as arguments
			// firstFragment.setArguments(getIntent().getExtras());

			// Add the fragment to the 'fragment_container' FrameLayout
			getSupportFragmentManager()
			.beginTransaction()
			.replace(R.id.fragment_container, firstFragment,
					"MAIN_BROWSER").commit();
		}

		menuHeading = (TextView) findViewById(R.id.menu_heading);

		Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
		setSupportActionBar(toolbar);

		DrawerLayout drawer = (DrawerLayout) findViewById(R.id.drawer_layout);
		android.support.v7.app.ActionBarDrawerToggle toggle = new android.support.v7.app.ActionBarDrawerToggle(
				this, drawer, toolbar, R.string.drawer_open, R.string.drawer_shut) {
			@Override
			public void onDrawerOpened(View drawerView) {
				NavigationView navigationView = (NavigationView) findViewById(R.id.nav_view);
				Menu nav_Menu = navigationView.getMenu();
				if (!hasAndroidMarket) {
					nav_Menu.findItem(R.id.rateme_menu).setVisible(false);
				} else {
					nav_Menu.findItem(R.id.rateme_menu).setVisible(true);
				}
			}
			@Override
			public void onDrawerClosed(View drawerView) {

			}
		};
		drawer.setDrawerListener(toggle);
		toggle.syncState();

		NavigationView navigationView = (NavigationView) findViewById(R.id.nav_view);
		navigationView.setNavigationItemSelectedListener(this);
	}

	public void generateErrorLog() {
		new GenerateLogs(MainActivity.this).execute(getFilesDir().getAbsolutePath());
	}

	/**
	 * Display a dialog to notify the user of prior crash
	 * 
	 * @param error
	 *            A generalized summary of the crash cause
	 */
	private void displayLogOutput(final String error) {
		AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);
		builder.setTitle(R.string.report_issue);
		builder.setMessage(error);
		builder.setPositiveButton(R.string.report,
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						generateErrorLog();
					}
				});
		builder.setNegativeButton(R.string.dismiss,
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						dialog.dismiss();
					}
				});
		builder.create();
		builder.show();
	}

    public static boolean isBiosExisting(Context context) {
        SharedPreferences mPrefs = PreferenceManager.getDefaultSharedPreferences(context);
        home_directory = mPrefs.getString("home_directory", home_directory);
        File bios = new File(home_directory, "data/dc_boot.bin");
        return bios.exists();
    }

    public static boolean isFlashExisting(Context context) {
        SharedPreferences mPrefs = PreferenceManager.getDefaultSharedPreferences(context);
        home_directory = mPrefs.getString("home_directory", home_directory);
        File flash = new File(home_directory, "data/dc_flash.bin");
        return flash.exists();
    }

	public void onGameSelected(Uri uri) {
		if (Config.readOutput("uname -a").equals(getString(R.string.error_kernel))) {
			MainActivity.showToastMessage(MainActivity.this, getString(R.string.unsupported), R.drawable.ic_notification, Toast.LENGTH_SHORT);
		}
		String msg = null;
		if (!isBiosExisting(MainActivity.this))
			msg = getString(R.string.missing_bios, home_directory);
		else if (!isFlashExisting(MainActivity.this))
			msg = getString(R.string.missing_flash, home_directory);

		if (msg != null) {
			launchBIOSdetection();
		} else {
			Config.nativeact = PreferenceManager.getDefaultSharedPreferences(getApplicationContext()).getBoolean(Config.pref_nativeact, Config.nativeact);
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD && Config.nativeact) {
				startActivity(new Intent("com.reicast.EMULATOR", uri, getApplicationContext(),
						GL2JNINative.class));
			} else {
				startActivity(new Intent("com.reicast.EMULATOR", uri, getApplicationContext(),
						GL2JNIActivity.class));
			}
		}
	}
	
	private void launchBIOSdetection() {
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle(R.string.bios_selection);
		builder.setPositiveButton(R.string.browse,
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						FileBrowser firstFragment = new FileBrowser();
						Bundle args = new Bundle();
						// args.putBoolean("ImgBrowse", false);
						// specify ImgBrowse option. true = images,
						// false = folders only
						args.putString("browse_entry", sdcard.toString());
						// specify a path for selecting folder
						// options
						args.putBoolean("games_entry", false);
						// selecting a BIOS folder, so this is not
						// games

						firstFragment.setArguments(args);
						// In case this activity was started with
						// special instructions from
						// an Intent, pass the Intent's extras to
						// the fragment as arguments
						// firstFragment.setArguments(getIntent().getExtras());

						// Add the fragment to the
						// 'fragment_container' FrameLayout
						getSupportFragmentManager()
						.beginTransaction()
						.replace(R.id.fragment_container,
								firstFragment,
								"MAIN_BROWSER")
								.addToBackStack(null).commit();
					}
				});
		builder.setNegativeButton(R.string.gdrive,
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						Toast.makeText(MainActivity.this, R.string.require_bios, Toast.LENGTH_SHORT).show();
					}
				});
		builder.create();
		builder.show();
	}

	public void onFolderSelected(Uri uri) {
		FileBrowser browserFrag = (FileBrowser) getSupportFragmentManager()
				.findFragmentByTag("MAIN_BROWSER");
		if (browserFrag != null) {
			if (browserFrag.isVisible()) {
				Log.d("reicast", "Main folder: " + uri.toString());
				// return;
			}
		}

		OptionsFragment optsFrag = new OptionsFragment();
		getSupportFragmentManager().beginTransaction()
				.replace(R.id.fragment_container, optsFrag, "OPTIONS_FRAG")
				.commit();
		return;
	}

	public void onMainBrowseSelected(String path_entry, boolean games) {
		FileBrowser firstFragment = new FileBrowser();
		Bundle args = new Bundle();
		args.putBoolean("ImgBrowse", false);
		// specify ImgBrowse option. true = images, false = folders only
		args.putString("browse_entry", path_entry);
		// specify a path for selecting folder options
		args.putBoolean("games_entry", games);
		// specify if the desired path is for games or data

		firstFragment.setArguments(args);
		// In case this activity was started with special instructions from
		// an Intent, pass the Intent's extras to the fragment as arguments
		// firstFragment.setArguments(getIntent().getExtras());

		// Add the fragment to the 'fragment_container' FrameLayout
		getSupportFragmentManager()
				.beginTransaction()
				.replace(R.id.fragment_container, firstFragment, "MAIN_BROWSER")
				.addToBackStack(null).commit();
	}

	@SuppressLint("NewApi")
	@Override
	public void setTitle(CharSequence title) {
		menuHeading.setText(title);
	}

	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (keyCode == KeyEvent.KEYCODE_BACK) {
			Fragment fragment = (FileBrowser) getSupportFragmentManager()
					.findFragmentByTag("MAIN_BROWSER");
			if (fragment != null && fragment.isVisible()) {
				boolean readyToQuit = true;
				if (fragment.getArguments() != null) {
					readyToQuit = fragment.getArguments().getBoolean(
							"ImgBrowse", true);
				}
				if (readyToQuit) {
					MainActivity.this.finish();
				} else {
					launchMainFragment(fragment);
				}
				return true;
			} else {
				launchMainFragment(fragment);
				return true;
			}

		}

		return super.onKeyDown(keyCode, event);
	}
	
	private void launchMainFragment(Fragment fragment) {
		fragment = new FileBrowser();
		Bundle args = new Bundle();
		args.putBoolean("ImgBrowse", true);
		args.putString("browse_entry", null);
		args.putBoolean("games_entry", false);
		fragment.setArguments(args);
		getSupportFragmentManager().beginTransaction()
				.replace(R.id.fragment_container, fragment, "MAIN_BROWSER")
				.addToBackStack(null).commit();
		setTitle(R.string.browser);
	}

	@Override
	protected void onPause() {
		super.onPause();
		InputFragment fragment = (InputFragment) getSupportFragmentManager()
				.findFragmentByTag("INPUT_FRAG");
		if (fragment != null && fragment.isVisible()) {
			if (fragment.moga != null) {
				fragment.moga.onPause();
			}
		}
	}

	@Override
	protected void onDestroy() {
		InputFragment fragment = (InputFragment) getSupportFragmentManager()
				.findFragmentByTag("INPUT_FRAG");
		if (fragment != null && fragment.isVisible()) {
			if (fragment.moga != null) {
				fragment.moga.onDestroy();
			}
		}
		super.onDestroy();
	}

	@Override
	protected void onResume() {
		super.onResume();
		InputFragment fragment = (InputFragment) getSupportFragmentManager()
				.findFragmentByTag("INPUT_FRAG");
		if (fragment != null && fragment.isVisible()) {
			if (fragment.moga != null) {
				fragment.moga.onResume();
			}
		}
		
		CloudFragment cloudfragment = (CloudFragment) getSupportFragmentManager()
				.findFragmentByTag("CLOUD_FRAG");
		if (cloudfragment != null && cloudfragment.isVisible()) {
			if (cloudfragment != null) {
				cloudfragment.onResume();
			}
		}
	}
	
	@Override
	public void onPostCreate(Bundle savedInstanceState) {
		super.onPostCreate(savedInstanceState);
	}

	@SuppressWarnings("StatementWithEmptyBody")
	@Override
	public boolean onNavigationItemSelected(MenuItem item) {
		// Handle navigation view item clicks here.
		DrawerLayout drawer = (DrawerLayout) findViewById(R.id.drawer_layout);

		switch (item.getItemId()) {

			case R.id.browser_menu:
				FileBrowser browseFrag = (FileBrowser) getSupportFragmentManager()
						.findFragmentByTag("MAIN_BROWSER");
				if (browseFrag != null) {
					if (browseFrag.isVisible()) {
						drawer.closeDrawer(GravityCompat.START);
						return true;
					}
				}
				browseFrag = new FileBrowser();
				Bundle args = new Bundle();
				args.putBoolean("ImgBrowse", true);
				args.putString("browse_entry", null);
				// specify a path for selecting folder options
				args.putBoolean("games_entry", false);
				// specify if the desired path is for games or data
				browseFrag.setArguments(args);
				getSupportFragmentManager()
						.beginTransaction()
						.replace(R.id.fragment_container, browseFrag,
								"MAIN_BROWSER").addToBackStack(null)
						.commit();
				setTitle(R.string.browser);

			case R.id.settings_menu:
				OptionsFragment optionsFrag = (OptionsFragment) getSupportFragmentManager()
						.findFragmentByTag("OPTIONS_FRAG");
				if (optionsFrag != null) {
					if (optionsFrag.isVisible()) {
						drawer.closeDrawer(GravityCompat.START);
						return true;
					}
				}
				optionsFrag = new OptionsFragment();
				getSupportFragmentManager()
						.beginTransaction()
						.replace(R.id.fragment_container,
								optionsFrag, "OPTIONS_FRAG")
						.addToBackStack(null).commit();
				setTitle(R.string.settings);

			case R.id.input_menu:
				InputFragment inputFrag = (InputFragment) getSupportFragmentManager()
						.findFragmentByTag("INPUT_FRAG");
				if (inputFrag != null) {
					if (inputFrag.isVisible()) {
						drawer.closeDrawer(GravityCompat.START);
						return true;
					}
				}
				inputFrag = new InputFragment();
				getSupportFragmentManager()
						.beginTransaction()
						.replace(R.id.fragment_container, inputFrag,
								"INPUT_FRAG").addToBackStack(null).commit();
				setTitle(R.string.input);

			case R.id.about_menu:
				AboutFragment aboutFrag = (AboutFragment) getSupportFragmentManager()
						.findFragmentByTag("ABOUT_FRAG");
				if (aboutFrag != null) {
					if (aboutFrag.isVisible()) {
						drawer.closeDrawer(GravityCompat.START);
						return true;
					}
				}
				aboutFrag = new AboutFragment();
				getSupportFragmentManager()
						.beginTransaction()
						.replace(R.id.fragment_container, aboutFrag,
								"ABOUT_FRAG").addToBackStack(null).commit();
				setTitle(R.string.about);

			case R.id.cloud_menu:
				CloudFragment cloudFrag = (CloudFragment) getSupportFragmentManager()
						.findFragmentByTag("CLOUD_FRAG");
				if (cloudFrag != null) {
					if (cloudFrag.isVisible()) {
						drawer.closeDrawer(GravityCompat.START);
						return true;
					}
				}
				cloudFrag = new CloudFragment();
				getSupportFragmentManager()
						.beginTransaction()
						.replace(R.id.fragment_container,
								cloudFrag, "CLOUD_FRAG")
						.addToBackStack(null).commit();
				setTitle(R.string.cloud);

			case R.id.rateme_menu:
				// vib.vibrate(50);
				startActivity(new Intent(Intent.ACTION_VIEW, Uri
						.parse("market://details?id=" + getPackageName())));
				//setTitle(R.string.rateme);

			case R.id.message_menu:
				generateErrorLog();
		}


		drawer.closeDrawer(GravityCompat.START);
		return true;
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus) {
		super.onWindowFocusChanged(hasFocus);
		if (hasFocus && Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
			getWindow().getDecorView().setSystemUiVisibility(
					View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
	                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
	                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
	                | View.SYSTEM_UI_FLAG_FULLSCREEN
	                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
	}

	public boolean isCallable(Intent intent) {
		List<ResolveInfo> list = getPackageManager().queryIntentActivities(
				intent, PackageManager.MATCH_DEFAULT_ONLY);
		return list.size() > 0;
	}
	
	public static void showToastMessage(Context context, String message,
			int resource, int duration) {
		LayoutInflater inflater = (LayoutInflater) context
				.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
		View layout = inflater.inflate(R.layout.toast_layout, null);

		ImageView image = (ImageView) layout.findViewById(R.id.image);
		image.setImageResource(resource);

		TextView text = (TextView) layout.findViewById(R.id.text);
		text.setText(message);

		DisplayMetrics metrics = new DisplayMetrics();
		WindowManager winman = (WindowManager) context
				.getSystemService(Context.WINDOW_SERVICE);
		winman.getDefaultDisplay().getMetrics(metrics);

		Toast toast = new Toast(context);
		toast.setGravity(Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL, 0,
						(int) (72 * metrics.density + 0.5f));
		toast.setDuration(duration);
		toast.setView(layout);
		toast.show();
	}
}
