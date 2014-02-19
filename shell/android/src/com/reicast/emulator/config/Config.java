package com.reicast.emulator.config;

import android.content.SharedPreferences;

import com.android.util.DreamTime;
import com.reicast.emulator.emu.JNIdc;

public class Config {

	public static boolean dynarecopt = true;
	public static boolean idleskip = true;
	public static boolean unstableopt = false;
	public static int cable = 3;
	public static int dcregion = 3;
	public static int broadcast = 4;
	public static boolean limitfps = true;
	public static boolean nobatch = false;
	public static boolean mipmaps = true;
	public static boolean widescreen = false;
	public static boolean subdivide = false;
	public static int frameskip = 0;
	public static boolean pvrrender = false;
	public static String cheatdisk = "null";

	/**
	 * Load the user configuration from preferences
	 * 
	 * @param sharedpreferences
	 *            The preference instance to load values from
	 */
	public static void getConfigurationPrefs(SharedPreferences mPrefs) {
		dynarecopt = mPrefs.getBoolean("dynarec_opt", dynarecopt);
		unstableopt = mPrefs.getBoolean("unstable_opt", unstableopt);
		cable = mPrefs.getInt("dc_cable", cable);
		dcregion = mPrefs.getInt("dc_region", dcregion);
		broadcast = mPrefs.getInt("dc_broadcast", broadcast);
		limitfps = mPrefs.getBoolean("limit_fps", limitfps);
		mipmaps = mPrefs.getBoolean("use_mipmaps", mipmaps);
		widescreen = mPrefs.getBoolean("stretch_view", widescreen);
		frameskip = mPrefs.getInt("frame_skip", frameskip);
		pvrrender = mPrefs.getBoolean("pvr_render", pvrrender);
		cheatdisk = mPrefs.getString("cheat_disk", cheatdisk);
	}

	/**
	 * Write configuration settings to the emulator
	 * 
	 */
	public static void loadConfigurationPrefs() {
		JNIdc.dynarec(dynarecopt ? 1 : 0);
		JNIdc.idleskip(idleskip ? 1 : 0);
		JNIdc.unstable(unstableopt ? 1 : 0);
		JNIdc.cable(cable);
		JNIdc.region(dcregion);
		JNIdc.broadcast(broadcast);
		JNIdc.limitfps(limitfps ? 1 : 0);
		JNIdc.nobatch(nobatch ? 1 : 0);
		JNIdc.mipmaps(mipmaps ? 1 : 0);
		JNIdc.widescreen(widescreen ? 1 : 0);
		JNIdc.subdivide(subdivide ? 1 : 0);
		JNIdc.frameskip(frameskip);
		JNIdc.pvrrender(pvrrender ? 1 : 0);
		JNIdc.cheatdisk(cheatdisk);
		JNIdc.dreamtime(DreamTime.getDreamtime());
	}

}
