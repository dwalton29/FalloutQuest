package com.falloutquest.app;
import java.io.File;
import java.util.ArrayList;
import java.util.List;

/** Data directly, or inside one untouched Steam install folder. */
final class GameInstall {
    enum Game {
        FALLOUT3("Fallout3", "Fallout3.esm"), NEW_VEGAS("FalloutNV", "FalloutNV.esm");
        final String folder, master;
        Game(String folder, String master) { this.folder = folder; this.master = master; }
    }
    static File child(File parent, String name) {
        File exact = new File(parent, name);
        if (exact.exists()) return exact;
        File[] children = parent.listFiles();
        if (children != null) for (File file : children)
            if (file.getName().equalsIgnoreCase(name)) return file;
        return exact;
    }
    static File findData(File gameFolder, Game game) {
        List<File> candidates = new ArrayList<>();
        File direct = child(gameFolder, "Data");
        if (readable(child(direct, game.master))) candidates.add(direct);
        File[] children = gameFolder.listFiles();
        if (children != null) for (File install : children) {
            if (!install.isDirectory() || install.equals(direct)) continue;
            File data = child(install, "Data");
            if (readable(child(data, game.master))) candidates.add(data);
        }
        // Do not silently choose between multiple installations.
        return candidates.size() == 1 ? candidates.get(0) : null;
    }
    static boolean readable(File file) {
        return file.isFile() && file.canRead() && file.length() > 0;
    }
    static String missing(File data, Game game) {
        if (data == null) return "No unique readable " + game.master +
            " found. Copy one Steam install into " + game.folder + ".";
        for (String name : new String[]{game.master, "Fallout - Meshes.bsa",
                "Fallout - Textures.bsa", "Fallout - Misc.bsa"})
            if (!readable(child(data, name))) return "Missing or unreadable: " + name;
        return "";
    }
}
