// Status window state management
// Communicates with C++ via window.chrome.webview.postMessage()

let currentView = 'status';
let currentServerName = 'Reflection';
let isConnected = false;

// Listen for messages from C++ host
if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', function(event) {
        try {
            const data = JSON.parse(event.data);
            handleHostMessage(data);
        } catch (e) {
            handleHostMessage(event.data);
        }
    });
}

function handleHostMessage(data) {
    switch (data.type) {
        case 'themeChanged':
            document.documentElement.className = 'theme-' + data.theme;
            break;

        case 'settingsUpdated':
            if (data.serverName) {
                currentServerName = data.serverName;
                document.getElementById('serverNameText').textContent = data.serverName;
            }
            if (data.theme) {
                updateThemeSelector(data.theme);
            }
            if (typeof data.runInBackground === 'boolean') {
                document.getElementById('toggleRunInBg').checked = data.runInBackground;
            }
            if (typeof data.launchAtLogin === 'boolean') {
                document.getElementById('toggleLaunchLogin').checked = data.launchAtLogin;
            }
            if (typeof data.autoUpdateEnabled === 'boolean') {
                document.getElementById('toggleAutoUpdate').checked = data.autoUpdateEnabled;
            }
            break;

        case 'connected':
            setConnectionState(true, data.deviceName);
            break;

        case 'disconnected':
            setConnectionState(false);
            break;

        case 'serverNameChanged':
            currentServerName = data.name;
            document.getElementById('serverNameText').textContent = data.name;
            break;

        case 'firewallStatus':
            updateFirewallBanner(!data.configured);
            break;

        case 'firewallResult':
            onFirewallResult(data.success);
            break;
    }
}

function sendToHost(message) {
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(JSON.stringify(message));
    }
}

// --- View Navigation (animated) ---

function showView(viewName) {
    var statusView = document.getElementById('view-status');
    var settingsView = document.getElementById('view-settings');
    var backBtn = document.getElementById('backBtn');
    var settingsBtn = document.getElementById('settingsBtn');
    var title = document.getElementById('toolbarTitle');

    var isForward = (viewName === 'settings');
    var outgoing = isForward ? statusView : settingsView;
    var incoming = isForward ? settingsView : statusView;

    // Animate out
    outgoing.classList.add(isForward ? 'view-slide-out-left' : 'view-slide-out-right');

    setTimeout(function() {
        outgoing.classList.remove('active', 'view-slide-out-left', 'view-slide-out-right');

        // Animate in
        incoming.classList.add('active', isForward ? 'view-slide-in-right' : 'view-slide-in-left');

        // Animate title change
        title.textContent = isForward ? 'Settings' : 'Reflection';
        title.classList.add('title-crossfade');
        setTimeout(function() {
            title.classList.remove('title-crossfade');
        }, 250);

        setTimeout(function() {
            incoming.classList.remove('view-slide-in-right', 'view-slide-in-left');
        }, 300);

        backBtn.style.display = isForward ? 'flex' : 'none';
        settingsBtn.style.display = isForward ? 'none' : 'flex';
    }, 200);

    currentView = viewName;
}

// --- Connection State ---

function setConnectionState(connected, deviceName) {
    var wasConnected = isConnected;
    isConnected = connected;
    var statusEl = document.getElementById('connectionStatus');
    var textEl = document.getElementById('connectionText');
    var dot = statusEl.querySelector('.status-dot');
    var disconnectRow = document.getElementById('disconnectRow');

    if (connected) {
        statusEl.classList.add('connected');
        dot.className = 'status-dot connected';
        textEl.textContent = 'Connected to ' + (deviceName || 'iPad');

        // Animate connection card with spring scale
        if (!wasConnected) {
            statusEl.classList.add('just-connected');
            setTimeout(function() {
                statusEl.classList.remove('just-connected');
            }, 400);
        }

        // Show disconnect button with entrance animation
        disconnectRow.style.display = 'block';
        disconnectRow.querySelector('.btn').classList.add('disconnect-enter');
    } else {
        statusEl.classList.remove('connected');
        dot.className = 'status-dot waiting';
        textEl.textContent = 'Waiting for connection...';

        // Animate disconnection
        if (wasConnected) {
            statusEl.classList.add('just-disconnected');
            setTimeout(function() {
                statusEl.classList.remove('just-disconnected');
            }, 300);
        }

        disconnectRow.style.display = 'none';
    }
}

function requestDisconnect() {
    sendToHost({ type: 'disconnect' });
}

// --- Name Editing ---

function startEditName() {
    var display = document.getElementById('nameDisplay');
    var editGroup = document.getElementById('nameEditGroup');
    var input = document.getElementById('nameEditInput');

    display.style.display = 'none';
    editGroup.classList.add('active', 'name-edit-enter');
    input.value = currentServerName;
    input.focus();
    input.select();

    setTimeout(function() {
        editGroup.classList.remove('name-edit-enter');
    }, 250);
}

function saveEditedName() {
    var input = document.getElementById('nameEditInput');
    var name = input.value.trim();

    if (name.length === 0) {
        // Shake the input on empty
        input.classList.add('shake');
        setTimeout(function() { input.classList.remove('shake'); }, 400);
        return;
    }

    if (name !== currentServerName) {
        currentServerName = name;
        var nameText = document.getElementById('serverNameText');
        nameText.textContent = name;
        sendToHost({ type: 'setServerName', name: name });

        // Flash the name display to confirm save
        var display = document.getElementById('nameDisplay');
        display.classList.add('save-flash');
        setTimeout(function() { display.classList.remove('save-flash'); }, 500);
    }

    cancelEditName();
}

function cancelEditName() {
    document.getElementById('nameDisplay').style.display = 'flex';
    document.getElementById('nameEditGroup').classList.remove('active');
}

// Handle Enter/Escape on name input
document.addEventListener('DOMContentLoaded', function() {
    var input = document.getElementById('nameEditInput');
    if (input) {
        input.addEventListener('keydown', function(e) {
            if (e.key === 'Enter') saveEditedName();
            if (e.key === 'Escape') cancelEditName();
        });
    }
});

// --- Settings ---

function setTheme(theme) {
    updateThemeSelector(theme);
    sendToHost({ type: 'setTheme', theme: theme });
}

function updateThemeSelector(theme) {
    var options = document.querySelectorAll('#themeSegmented .segmented-option');
    options.forEach(function(opt) {
        if (opt.getAttribute('data-value') === theme) {
            opt.classList.add('active');
        } else {
            opt.classList.remove('active');
        }
    });
}

function setRunInBackground(enabled) {
    sendToHost({ type: 'setRunInBackground', enabled: enabled });
}

function setLaunchAtLogin(enabled) {
    sendToHost({ type: 'setLaunchAtLogin', enabled: enabled });
}

function setAutoUpdate(enabled) {
    sendToHost({ type: 'setAutoUpdate', enabled: enabled });
}

function checkForUpdates() {
    sendToHost({ type: 'checkForUpdates' });
}

// --- Window Controls ---

function minimizeWindow() {
    sendToHost({ type: 'minimizeWindow' });
}

function closeWindow() {
    sendToHost({ type: 'closeWindow' });
}

// Drag handle
(function() {
    document.addEventListener('DOMContentLoaded', function() {
        var handle = document.getElementById('dragHandle');
        if (handle) {
            handle.addEventListener('mousedown', function(e) {
                e.preventDefault();
                sendToHost({ type: 'startDrag' });
            });
        }
    });
})();

// --- Broadcast (manual mDNS re-announce with rate limiter) ---

var lastBroadcastTime = 0;
var BROADCAST_COOLDOWN_MS = 10000; // 10-second cooldown

function triggerBroadcast() {
    var now = Date.now();
    var elapsed = now - lastBroadcastTime;

    if (elapsed < BROADCAST_COOLDOWN_MS) {
        // Rate limited -- show cooldown feedback
        var btn = document.getElementById('broadcastBtn');
        btn.classList.add('shake');
        setTimeout(function() { btn.classList.remove('shake'); }, 400);
        return;
    }

    lastBroadcastTime = now;
    sendToHost({ type: 'forceReannounce' });

    // Visual feedback: spin the broadcast icon
    var btn = document.getElementById('broadcastBtn');
    btn.classList.add('spin');
    btn.disabled = true;
    setTimeout(function() {
        btn.classList.remove('spin');
        btn.disabled = false;
    }, 1000);
}

// --- Firewall Banner ---

function updateFirewallBanner(show) {
    var banner = document.getElementById('firewallBanner');
    if (banner) {
        if (show) {
            banner.classList.add('visible');
        } else {
            banner.classList.remove('visible');
        }
    }
}

function grantFirewallFromBanner() {
    var btn = document.getElementById('bannerGrantBtn');
    btn.disabled = true;
    btn.textContent = 'Requesting...';
    sendToHost({ type: 'configureFirewall' });
}

function onFirewallResult(success) {
    var btn = document.getElementById('bannerGrantBtn');
    if (success) {
        updateFirewallBanner(false);
    } else {
        btn.disabled = false;
        btn.textContent = 'Try Again';
    }
}
