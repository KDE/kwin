// ==============================================================
// Ok, *some* apps have really long and nasty window captions
// this looks clutterd, so we allow to crop them a bit and remove
// any information considered to be useless
// ==============================================================

function isBrowser(appName) {
    if (appName.toLowerCase() == "konqueror")
        return true;
    if (appName.toLowerCase() == "rekonq")
        return true;
    if (appName.toLowerCase() == "qupzilla")
        return true;
    if (appName.toLowerCase() == "chromium")
        return true;
    if (appName.toLowerCase() == "firefox")
        return true;
    if (appName.toLowerCase() == "opera")
        return true;
    if (appName.toLowerCase() == "arora")
        return true;
    if (appName.toLowerCase() == "mozilla")
        return true;
    return false;
}

(function(title, wm_name, wm_class) {
    var ret;

    // == 1st off ======================================================================
    // we assume the part beyond the last dash (if any) to be the
    // uninteresting one (usually it's the apps name, if there's add info, that's
    // more important to the user)
    // --------------------------------------------------------------------------------
    var lastPos = title.lastIndexOf(" – "); //  U+2013 "EN DASH"
    if (lastPos > -1)
        ret = title.slice(0, lastPos);
    else {
        lastPos = title.lastIndexOf(" - "); // ASCII Dash
        if (lastPos > -1)
            ret = title.slice(0, lastPos);
        else {
            lastPos = title.lastIndexOf(" — "); // U+2014 "EM DASH"
            if (lastPos > -1)
                ret = title.slice(0, lastPos);
            else
                ret = title;
        }
    }

    // == 2nd =========================================================================
    // Browsers set the caption to "<html><title/></html> - appname"
    // Now the page titles can be ridiculously looooong, especially on news pages
    // -------------------------------------------------------------------------------
    if (isBrowser(wm_name)) {
        var parts = ret.split(" - ");
        ret = "";
        if (parts.length > 2) {  // select last two if 3 or more sects, prelast otherwise
            for (i = Math.max(0,parts.length-2); i < parts.length - 1; ++i)
                ret = ret + parts[i] + " - ";
            ret = ret + parts[parts.length - 1];
        } else {
            ret = ret + parts[Math.max(0,parts.length-2)];
        }
    }

    // 3rd ============================================================================
    // if there're any details left, cut of stuff by ": ",
    // we remove them as well
    // --------------------------------------------------------------------------------
    var lastPos = ret.lastIndexOf(": ");
    if (lastPos > -1)
        ret = title.slice(0, lastPos);

    // 4th ============================================================================
    // if this is a http url, please get rid of protocol, assuming the user knows or doesn't care
    // --------------------------------------------------------------------------------
    ret = ret.replace("http://", '');

    // finally =========================================================================
    // if the remaining string still contains the app name (which should have been removed),
    // please shape away additional info like compile time, version numbers etc.
    // we usitlize the caption string to preserve CapiTaliZation
    // -----------------------------------------------------------------------------------
    lastPos = ret.indexOf(RegExp("\\b" + wm_name + "\\b"));
    if (lastPos > -1)
        ret = ret.substr(lastPos, wm_name.length);
    else {
        lastPos = ret.indexOf(RegExp("\\b" + wm_class + "\\b"));
        if (lastPos > -1)
            ret = ret.substr(lastPos, wm_class.length);
    }

    if (ret.length == 0)
        ret = title; // something _terribly_ went wrong -> fall back to the original string

    // but in any case get replace the stupid [modified] hint by just an asterisk
    ret = ret.replace("[modified]", "❖");

    // in general, remove leading [and trailing] blanks and special chars
    // ret = ret.replace(/^\W*/, ''); - this does not work - Umlauts and pot. other chars are considered "non Word"
    ret = ret.replace(/^\s*/, '').replace(/\s*$/, '');
    return ret;
})