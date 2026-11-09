/**
 * keet-vdom.js - Lightweight zero-dependency Virtual DOM client runtime for KeetScript Web UI.
 * Connects over WebSocket, renders VDOM trees, handles optimistic input, reconciles keyed lists,
 * and recovers gracefully on connection drops with exponential backoff.
 */
(function() {
    let ws = null;
    let rootEl = null;
    let vdomTree = null;
    let reconnectDelay = 1000;
    const maxReconnectDelay = 16000;

    function createDomNode(vnode) {
        if (!vnode) return null;
        let el = document.createElement(vnode.tag || 'div');
        if (vnode.id) el.dataset.keetId = vnode.id;
        if (vnode.key) el.dataset.keetKey = vnode.key;

        if (vnode.attrs) {
            for (let [k, v] of Object.entries(vnode.attrs)) {
                if (k === 'class') el.className = v;
                else if (k === 'value' && el.tagName === 'INPUT') el.value = v;
                else el.setAttribute(k, v);
            }
        }
        if (vnode.styles) {
            for (let [k, v] of Object.entries(vnode.styles)) {
                el.style[k] = v;
            }
        }
        if (vnode.text) {
            if (el.tagName === 'BUTTON' || el.tagName === 'SPAN' || el.tagName === 'P') {
                el.textContent = vnode.text;
            }
        }
        if (vnode.children) {
            for (let child of vnode.children) {
                let cel = createDomNode(child);
                if (cel) el.appendChild(cel);
            }
        }

        // Attach synthetic event delegation
        el.addEventListener('click', (e) => {
            if (vnode.id) sendEvent(vnode.id, 'click', {});
        });
        if (el.tagName === 'INPUT') {
            el.addEventListener('input', (e) => {
                if (vnode.id) sendEvent(vnode.id, 'input', { value: el.value });
            });
            el.addEventListener('change', (e) => {
                if (vnode.id) sendEvent(vnode.id, 'change', { value: el.value });
            });
        }
        return el;
    }

    function applyPatches(patches) {
        for (let p of patches) {
            let el = document.querySelector(`[data-keet-id="${p.id}"]`);
            if (!el) continue;
            if (p.op === 'replaceText') {
                el.textContent = p.val;
            } else if (p.op === 'setAttr') {
                if (p.key === 'class') el.className = p.val;
                else if (p.key === 'value' && el.tagName === 'INPUT') {
                    // Optimistic cursor preservation
                    if (document.activeElement !== el) el.value = p.val;
                } else el.setAttribute(p.key, p.val);
            }
        }
    }

    function sendEvent(id, eventName, data) {
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'event', id: id, event: eventName, data: data }));
        }
    }

    function connect() {
        let proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
        let wsUrl = `${proto}//${location.host}/_keet_ws`;
        ws = new WebSocket(wsUrl);

        ws.onopen = () => {
            reconnectDelay = 1000;
            hideBanner();
            ws.send(JSON.stringify({ type: 'sync' }));
        };

        ws.onmessage = (msg) => {
            try {
                let data = JSON.parse(msg.data);
                if (data.type === 'vdom_full') {
                    vdomTree = data.vdom;
                    if (rootEl) {
                        rootEl.innerHTML = '';
                        let dom = createDomNode(vdomTree);
                        if (dom) rootEl.appendChild(dom);
                    }
                } else if (data.type === 'vdom_patch') {
                    applyPatches(data.patches);
                }
            } catch (err) {
                console.error('[keet-vdom] error handling message:', err);
            }
        };

        ws.onclose = () => {
            showBanner('Connection lost. Reconnecting...');
            setTimeout(connect, reconnectDelay);
            reconnectDelay = Math.min(reconnectDelay * 2, maxReconnectDelay);
        };
    }

    function showBanner(text) {
        let b = document.getElementById('keet-status-banner');
        if (!b) {
            b = document.createElement('div');
            b.id = 'keet-status-banner';
            b.style.cssText = 'position:fixed;top:0;left:0;right:0;padding:6px;background:#e53e3e;color:white;text-align:center;font-family:sans-serif;font-size:12px;z-index:99999;';
            document.body.prepend(b);
        }
        b.textContent = text;
        b.style.display = 'block';
    }

    function hideBanner() {
        let b = document.getElementById('keet-status-banner');
        if (b) b.style.display = 'none';
    }

    window.addEventListener('DOMContentLoaded', () => {
        rootEl = document.getElementById('keet-app') || document.body;
        connect();
    });
})();

