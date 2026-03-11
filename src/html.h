// html.h
// Skupni HTML nav bar za vse strani CEE
// IP naslovi se berejo iz config.h

#ifndef HTML_H
#define HTML_H

#include "config.h"

// ============================================================
// Skupni CSS za nav bar (enkrat definiran, uporabljen v vseh straneh)
// ============================================================
const char HTML_NAV_CSS[] PROGMEM =
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:Arial,sans-serif;font-size:15px;background:#101010;color:#e0e0e0;}"
    "nav{background:#1a1a1a;border-bottom:1px solid #333;padding:8px 16px;"
        "display:flex;gap:8px;flex-wrap:wrap;align-items:center;"
        "box-shadow:0 2px 8px rgba(0,0,0,.4);}"
    ".nav-home{background:#4caf50;color:#fff;border-radius:5px;padding:7px 11px;"
        "font-size:17px;text-decoration:none;line-height:1;display:inline-block;}"
    ".nav-home:hover{background:#66bb6a;}"
    ".nav-btn{background:#2a2a2a;color:#4da6ff;border:1px solid #4da6ff;"
        "border-radius:5px;padding:7px 14px;font-size:13px;font-weight:bold;"
        "text-decoration:none;white-space:nowrap;}"
    ".nav-btn:hover,.nav-btn.current{background:#4da6ff;color:#101010;}"
    ".nav-sep{width:1px;background:#444;align-self:stretch;margin:0 4px;}"
    ".nav-ext{background:#2a2a2a;color:#aaa;border:1px solid #444;"
        "border-radius:5px;padding:7px 14px;font-size:13px;font-weight:bold;"
        "text-decoration:none;white-space:nowrap;}"
    ".nav-ext:hover{background:#333;color:#e0e0e0;border-color:#666;}"
    ".wrap{max-width:1400px;margin:0 auto;padding:16px;}"
    "h1{font-size:22px;color:#e0e0e0;margin:0;}"
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:16px;margin-top:16px;}"
    ".card{background:#1a1a1a;border:1px solid #333;border-radius:8px;padding:18px;"
        "box-shadow:0 2px 8px rgba(0,0,0,.3);}"
    ".card h2{color:#4da6ff;margin-top:0;margin-bottom:14px;font-size:18px;"
        "border-bottom:2px solid #4da6ff;padding-bottom:5px;}"
    ".status-item{display:flex;justify-content:space-between;margin-bottom:8px;"
        "padding:5px 0;border-bottom:1px solid #333;}"
    ".status-item:last-child{border-bottom:none;}"
    ".status-label{font-weight:bold;color:#ccc;}"
    ".status-value{color:#e0e0e0;}"
    ".message{padding:15px;border-radius:4px;margin-bottom:20px;font-weight:bold;}"
    ".message.success{background:#44ff44;color:#101010;border:1px solid #22dd22;}"
    ".message.error{background:#ff4444;color:#fff;border:1px solid #dd2222;}";

// ============================================================
// Nav bar HTML (enkrat definiran, uporabljen v vseh straneh)
// Gumbi za CEE strani (modri) + separator + gumbi za druge enote (sivi)
// ============================================================
const char HTML_NAV_BAR[] PROGMEM =
    "<nav>"
        "<a href='/' class='nav-home' title='CEE domača stran'>&#127968;</a>"
        "<a href='/' class='nav-btn'>Domača</a>"
        "<a href='/settings' class='nav-btn'>Nastavitve</a>"
        "<a href='/help' class='nav-btn'>Pomoč</a>"
        "<span class='nav-sep'></span>"
        "<a href='http://" IP_REW "/' class='nav-ext'>REW</a>"
        "<a href='http://" IP_UT_DEW "/' class='nav-ext'>UT</a>"
        "<a href='http://" IP_KOP_DEW "/' class='nav-ext'>KOP</a>"
        "<a href='http://" IP_SEW1 "/' class='nav-ext'>SEW1</a>"
        "<a href='http://" IP_SEW2 "/' class='nav-ext'>SEW2</a>"
        "<a href='http://" IP_SEW3 "/' class='nav-ext'>SEW3</a>"
        "<a href='http://" IP_SEW4 "/' class='nav-ext'>SEW4</a>"
        "<a href='http://" IP_SEW5 "/' class='nav-ext'>SEW5</a>"
    "</nav>"
    "<script>"
        // Avtomatično označi trenutno aktivno stran
        "document.addEventListener('DOMContentLoaded',function(){"
            "var p=window.location.pathname.replace(/\\/$/,'')||'/';"
            "document.querySelectorAll('.nav-btn').forEach(function(b){"
                "if(b.getAttribute('href')===p)b.classList.add('current');"
            "});"
        "});"
    "</script>";

#endif // HTML_H
