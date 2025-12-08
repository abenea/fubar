function showHide(win) {
    if (win === undefined) return;
    if (win.minimized) {
        win.desktops = [workspace.currentDesktop];
        win.skipTaskbar = false;
        workspace.raiseWindow(win);
        workspace.activeWindow = win;
    } else {
        win.minimized = true;
        win.skipTaskbar = true;
    }
}

function run() {
    showHide(workspace.windowList().find(e => e.resourceClass === 'fubar'));
}

registerShortcut("ShowHideFubar", "Show/Hide fubar", "Meta+W", run);
