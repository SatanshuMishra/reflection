// Onboarding page state machine
// Communicates with C++ via window.chrome.webview.postMessage()

let currentPage = 0;
const pages = ['page-welcome', 'page-name', 'page-firewall'];

// Listen for messages from C++ host
if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', function(event) {
        try {
            const data = JSON.parse(event.data);
            handleHostMessage(data);
        } catch (e) {
            // Try as plain object
            handleHostMessage(event.data);
        }
    });
}

function handleHostMessage(data) {
    switch (data.type) {
        case 'themeChanged':
            document.documentElement.className = 'theme-' + data.theme;
            break;

        case 'setRandomName':
            document.getElementById('deviceName').value = data.name;
            break;

        case 'firewallResult':
            onFirewallResult(data.success, data.message);
            break;
    }
}

function sendToHost(message) {
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(JSON.stringify(message));
    }
}

// --- Page Navigation ---

function goToPage(targetPage) {
    if (targetPage === currentPage) return;
    if (targetPage < 0 || targetPage >= pages.length) return;

    const currentEl = document.getElementById(pages[currentPage]);
    const targetEl = document.getElementById(pages[targetPage]);
    const isForward = targetPage > currentPage;

    // Animate out current page
    currentEl.classList.add(isForward ? 'slide-out-left' : 'slide-out-right');

    setTimeout(function() {
        currentEl.classList.remove('active', 'slide-out-left', 'slide-out-right');

        // Animate in target page
        targetEl.classList.add('active', isForward ? 'slide-in-right' : 'slide-in-left');

        setTimeout(function() {
            targetEl.classList.remove('slide-in-right', 'slide-in-left');
        }, 350);

        // Update step indicator
        updateStepIndicator(targetPage);
        currentPage = targetPage;

        // Page-specific init
        if (targetPage === 1) {
            // Focus the name input
            setTimeout(function() {
                document.getElementById('deviceName').focus();
            }, 400);
        }
        if (targetPage === 2) {
            // Stagger-animate firewall feature list items
            animateFirewallFeatures();
        }
    }, 350);
}

function updateStepIndicator(activeStep) {
    const dots = document.querySelectorAll('.step-dot');
    dots.forEach(function(dot, index) {
        if (index === activeStep) {
            dot.classList.remove('inactive');
            dot.classList.add('active');
        } else {
            dot.classList.remove('active');
            dot.classList.add('inactive');
        }
    });
}

// --- Name Page ---

function shuffleName() {
    const btn = document.getElementById('shuffleBtn');
    btn.classList.add('spin');
    setTimeout(function() {
        btn.classList.remove('spin');
    }, 400);

    sendToHost({ type: 'generateName' });
}

function saveName() {
    const nameInput = document.getElementById('deviceName');
    const name = nameInput.value.trim();

    if (name.length === 0) {
        nameInput.style.borderColor = 'var(--error)';
        setTimeout(function() {
            nameInput.style.borderColor = '';
        }, 1500);
        return;
    }

    sendToHost({ type: 'setServerName', name: name });
    goToPage(2);
}

// Handle Enter key on name input
document.addEventListener('DOMContentLoaded', function() {
    const nameInput = document.getElementById('deviceName');
    if (nameInput) {
        nameInput.addEventListener('keydown', function(e) {
            if (e.key === 'Enter') {
                saveName();
            }
        });
    }

    // Request initial random name from host
    sendToHost({ type: 'generateName' });

    // Animate welcome page entrance with staggered delays
    animateWelcomeEntrance();
});

function animateWelcomeEntrance() {
    var welcome = document.querySelector('.welcome-content');
    if (!welcome) return;

    var children = welcome.children;
    for (var i = 0; i < children.length; i++) {
        children[i].classList.add('entrance-up', 'stagger-' + (i + 1));
    }
}

// --- Firewall Page ---

function requestFirewall() {
    var btn = document.getElementById('allowBtn');
    btn.disabled = true;
    btn.innerHTML = '<span class="spinner"></span> Requesting...';

    // Show status bar with spinner + "Configuring..." message
    var status = document.getElementById('firewallStatus');
    status.className = 'firewall-status pending visible status-slide-in';
    document.getElementById('firewallStatusIcon').innerHTML =
        '<span class="spinner" style="width:16px;height:16px;border-width:2px;"></span>';
    document.getElementById('firewallStatusText').textContent = 'Configuring...';

    // Hide skip while configuring
    document.getElementById('skipRow').style.display = 'none';

    sendToHost({ type: 'configureFirewall' });
}

function onFirewallResult(success, message) {
    var status = document.getElementById('firewallStatus');
    var statusText = document.getElementById('firewallStatusText');
    var statusIcon = document.getElementById('firewallStatusIcon');
    var firewallActions = document.getElementById('firewallActions');
    var completeActions = document.getElementById('completeActions');
    var skipRow = document.getElementById('skipRow');

    // Restore the SVG icon (replace spinner)
    var checkSvg = '<svg class="status-icon check-appear" viewBox="0 0 24 24" fill="currentColor">' +
        '<path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/>' +
        '</svg>';
    var errorSvg = '<svg class="status-icon shake" viewBox="0 0 24 24" fill="currentColor">' +
        '<path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/>' +
        '</svg>';

    if (success) {
        status.className = 'firewall-status success visible status-slide-in';
        statusIcon.innerHTML = checkSvg;
        statusText.textContent = 'Access granted';

        // Replace buttons with "Start Reflecting"
        firewallActions.style.display = 'none';
        skipRow.style.display = 'none';
        completeActions.style.display = 'flex';
        completeActions.classList.add('spring-in');
    } else {
        status.className = 'firewall-status error visible status-slide-in';
        statusIcon.innerHTML = errorSvg;
        statusText.textContent = message || 'Cancelled or failed — you can try again or skip';

        // Re-enable the allow button with shake feedback
        var btn = document.getElementById('allowBtn');
        btn.disabled = false;
        btn.innerHTML = 'Try Again';
        btn.classList.add('shake');
        setTimeout(function() { btn.classList.remove('shake'); }, 500);

        // Show skip option
        skipRow.style.display = 'flex';
    }
}

function completeOnboarding() {
    sendToHost({ type: 'onboardingComplete' });
}

// --- Animation Helpers ---

function animateFirewallFeatures() {
    var items = document.querySelectorAll('.firewall-features li');
    items.forEach(function(item, i) {
        item.style.opacity = '0';
        item.classList.remove('feature-enter');
        setTimeout(function() {
            item.classList.add('feature-enter');
            item.style.animationDelay = (i * 0.08) + 's';
        }, 100);
    });
}
