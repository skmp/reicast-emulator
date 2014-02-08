package com.reicast.emulator;

import java.util.Calendar;

public class Utils {
	
	private static long dreamRTC = ((20 * 365 + 5) * 86400);

	public static long getDreamtime(){
		Calendar cal = Calendar.getInstance();
		int utcOffset = cal.get(Calendar.ZONE_OFFSET)+cal.get(Calendar.DST_OFFSET);
		return (System.currentTimeMillis() / 1000) + dreamRTC + utcOffset/1000;
	}

}
