package com.defold.externaldirectory;

import android.content.Context;
import android.os.Environment;
import android.util.Log;

public class ExternalDirectory {
    public static String get(Context context) {
		return context.getExternalFilesDir(null).getPath();
    }
}
