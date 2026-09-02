#ifndef HTML_PAGES_H
#define HTML_PAGES_H

#include "config.h"

static String htmlEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else if (c == '\'') out += "&#39;";
    else out += c;
  }
  return out;
}

static String wireSelectHtml(int idx, const String& label) {
  bool pending = !isStandbyActive && (currentStates[idx] != targetStates[idx]);
  char t = sanitizeAntennaState(targetStates[idx]);
  String sClass = pending ? "w-pending" : (String("w-") + t);
  String selA = (t == 'A') ? " selected" : "";
  String selB = (t == 'B') ? " selected" : "";
  String selC = (t == 'C') ? " selected" : "";
  String optC = "Vypnutý";
  String optA = "Žiarič";
  String optB = "Reflektor";
  String sync = pending ? "<span class='sync' title='Čaká sa na potvrdenie zo stožiara'>SYNC</span>" : "";
  return String("<label class='w ") + sClass + "'><span class='id'>" + label + "</span>" + sync +
    "<select name='l" + String(idx) + "'><option value='C'" + selC + ">" + optC +
    "</option><option value='A'" + selA + ">" + optA +
    "</option><option value='B'" + selB + ">" + optB + "</option></select></label>";
}

static void hubFields(String& status, String& color, String& t1, String& t2) {
  t1 = "--.-";
  t2 = "--.-";
  status = "OK";
  color = "#3ee0d8";
  if (isStandbyActive) {
    status = "SPÁNOK";
    color = "#8B6CFF";
  } else if (waitingForReply) {
    status = "ČAKÁ SA";
    color = "#ff9800";
  } else if (nodeAnomaly == "OFFLINE") {
    status = "TIMEOUT";
    color = "#ff3333";
  } else {
    t1 = (nodeTempMCU > 900.0) ? "CHYBA" : String(nodeTempMCU, 1) + "°C";
    t2 = (nodeTempRelay > 900.0) ? "CHYBA" : String(nodeTempRelay, 1) + "°C";
    if (nodeAnomaly == "OK" || nodeAnomaly == "NONE") {
      status = "OK";
    } else {
      status = nodeAnomaly;
      color = "#ff3333";
    }
  }
}

String jsonHubStatus() {
  String status, color, t1, t2;
  hubFields(status, color, t1, t2);
  String j = "{\"s\":\"";
  j += htmlEscape(status);
  j += "\",\"c\":\"";
  j += color;
  j += "\",\"t1\":\"";
  j += htmlEscape(t1);
  j += "\",\"t2\":\"";
  j += htmlEscape(t2);
  j += "\"}";
  return j;
}

static void sendP(const char* s) { server.sendContent(s, strlen(s)); }
static void sendS(const String& s) { if (s.length()) server.sendContent(s); }

void sendHTML(bool loggedIn) {
  server.sendHeader("Connection", "close");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");
  sendP("<!DOCTYPE html><html lang='sk'><head><meta charset='UTF-8'>");
  sendP("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  sendP("<title>Stožiar</title><style>");
  sendP(":root{--void:#06080b;--glass:#0b1116;--plate:#101820;--ink:#e4eef0;--mute:#6e828c;--cyan:#3ee0d8;--amber:#d4eef2;--rule:rgba(62,224,216,.22);--A:#ff3b3b;--B:#3d7dff;--C:#d4eef2;--pend:#ff9800;--ok:#3ee0d8;--sleep:#8B6CFF}");
  sendP("*{box-sizing:border-box}html,body{margin:0;color:var(--ink);overflow-x:hidden}");
  sendP("body{min-height:100vh;font:13px/1.4 Consolas,'Lucida Console',monospace;padding:22px 18px 40px;background:radial-gradient(ellipse 80% 46% at 50% -8%,rgba(62,224,216,.1),transparent 52%),repeating-linear-gradient(90deg,transparent 0 42px,rgba(62,224,216,.03) 42px 43px),repeating-linear-gradient(0deg,transparent 0 42px,rgba(212,238,242,.02) 42px 43px),var(--void)}");
  sendP(".deck{position:relative;max-width:1080px;width:100%;margin:0 auto;background:linear-gradient(165deg,rgba(16,24,32,.97),rgba(7,10,14,.98));border:1px solid var(--rule);padding:18px 22px 22px;box-shadow:0 0 48px rgba(62,224,216,.07),inset 0 1px 0 rgba(62,224,216,.12)}");
  sendP(".deck:before,.deck:after{content:'';position:absolute;width:16px;height:16px;pointer-events:none;z-index:3}");
  sendP(".deck:before{top:-1px;left:-1px;border-top:2px solid var(--cyan);border-left:2px solid var(--cyan);box-shadow:0 0 10px var(--cyan)}");
  sendP(".deck:after{right:-1px;bottom:-1px;border-right:2px solid var(--amber);border-bottom:2px solid var(--amber);box-shadow:0 0 10px var(--amber)}");
  sendP(".mast{position:relative;display:flex;justify-content:space-between;align-items:flex-end;gap:12px;margin-bottom:4px;padding-bottom:12px;border-bottom:1px solid var(--rule)}");
  sendP(".mast:before{content:'';position:absolute;top:-19px;right:-23px;width:14px;height:14px;border-top:2px solid var(--amber);border-right:2px solid var(--amber)}");
  sendP(".tag{margin:0 0 5px;font-size:9px;letter-spacing:.24em;color:var(--cyan);text-shadow:0 0 8px rgba(62,224,216,.5)}");
  sendP("h1{margin:0;font-size:22px;font-weight:600;letter-spacing:.2em;text-transform:uppercase;text-shadow:0 0 18px rgba(62,224,216,.35)}");
  sendP(".brand{display:flex;align-items:center;gap:10px;flex-wrap:wrap}");
  sendP(".call{font:11px Consolas,monospace;color:var(--mute);text-align:right;letter-spacing:.04em}");
  sendP(".call b{display:block;color:var(--amber);font-weight:600;letter-spacing:.1em;text-shadow:0 0 8px rgba(212,238,242,.4)}");
  sendP(".hudrow{display:flex;flex-wrap:wrap;gap:6px;margin:0}");
  sendP(".hudrow div{display:inline-flex;align-items:baseline;gap:5px;padding:3px 9px;background:var(--glass);border:1px solid var(--rule);clip-path:polygon(5px 0,100% 0,calc(100% - 5px) 100%,0 100%)}");
  sendP(".hudrow span{font-size:8px;letter-spacing:.12em;color:var(--mute)}");
  sendP(".hudrow b{font-size:10px;letter-spacing:.08em;color:var(--cyan);font-weight:600}");
  sendP(".hudrow div:nth-child(even) b{color:var(--amber)}");
  sendP(".warn{color:#ff6a3c;font-size:11px;margin:8px 0 0;letter-spacing:.08em}");
  sendP(".k{font-size:9px;letter-spacing:.2em;text-transform:uppercase;color:var(--mute)}");
  sendP(".rig{position:relative;margin:8px 0 6px;padding:18px 38px 16px;overflow:visible}");
  sendP(".schem{position:absolute;inset:0;width:100%;height:100%;pointer-events:none;z-index:0;overflow:visible}");
  sendP(".chev{opacity:.2;animation:chevPulse 2.6s linear infinite;filter:drop-shadow(0 0 2px currentColor)}");
  sendP(".chev.still{animation:none!important;opacity:.62;filter:drop-shadow(0 0 1px currentColor)}");
  sendP("@keyframes chevPulse{0%,100%{opacity:.16}6%{opacity:.28}14%{opacity:.62}22%{opacity:.95}30%{opacity:1}38%{opacity:.7}48%{opacity:.32}60%{opacity:.16}}");
  sendP(".antenna{position:relative;z-index:1;display:grid;grid-template-columns:1fr 280px 1fr;gap:14px;align-items:center}");
  sendP(".col{display:flex;flex-direction:column;gap:45px;min-width:0;align-items:center}");
  sendP(".pair{display:grid;grid-template-columns:max-content max-content;gap:72px;justify-content:center}");
  sendP(".tip{display:flex;justify-content:center}");
  sendP(".hub{--feed:var(--ok);position:relative;width:280px;height:280px;margin:0 auto;overflow:visible;background:transparent}");
  sendP(".core{position:absolute;inset:0;border-radius:50%;z-index:1;background:transparent;border:0;box-shadow:none}");
  sendP(".tick-clip{position:absolute;width:80%;aspect-ratio:620/530;left:50%;top:61%;transform:translate(-50%,-50%);z-index:2;pointer-events:none;overflow:hidden;-webkit-clip-path:polygon(9.95% 4.81%,90.19% 5.04%,95.42% 15.52%,55.26% 88.68%,44.61% 88.68%,4.44% 15.52%);clip-path:polygon(9.95% 4.81%,90.19% 5.04%,95.42% 15.52%,55.26% 88.68%,44.61% 88.68%,4.44% 15.52%)}");
  sendP(".tick-clip:after{content:'';position:absolute;left:49.93%;top:37.42%;width:48%;aspect-ratio:1;translate:-50% -50%;border-radius:50%;border:1px dashed color-mix(in srgb,var(--feed) 30%,transparent);opacity:.42}");
  sendP(".ticks{position:absolute;display:block;left:49.93%;top:37.42%;width:70%;aspect-ratio:1;translate:-50% -50%;border-radius:50%;pointer-events:none;opacity:.85;background:repeating-conic-gradient(from 2deg,var(--feed) 0 .28deg,transparent .28deg 12deg);-webkit-mask:radial-gradient(farthest-side,transparent 86%,#000 87%,#000 91%,transparent 92%);mask:radial-gradient(farthest-side,transparent 86%,#000 87%,#000 91%,transparent 92%);animation:orbit 88s linear infinite reverse}");
  sendP("@keyframes orbit{to{transform:rotate(360deg)}}");
  sendP(".cradle{position:absolute;width:80%;height:auto;left:50%;top:61%;transform:translate(-50%,-50%);pointer-events:none;z-index:2;overflow:visible;filter:drop-shadow(0 0 5px color-mix(in srgb,var(--cyan) 45%,transparent))}");
  sendP(".feed{position:absolute;left:50%;top:51.2%;transform:translate(-50%,-50%);z-index:3;width:36%;padding:0;border:0;background:none;box-shadow:none;display:flex;flex-direction:column;align-items:center;justify-content:center;text-align:center;pointer-events:none}");
  sendP(".feed .mode{font:9px Consolas,'Lucida Console',monospace;margin:0 0 2px;letter-spacing:.24em;opacity:.75}");
  sendP(".tels{display:grid;grid-template-columns:max-content max-content;column-gap:3px;row-gap:2.5px;width:max-content;margin:4px auto 0;align-items:end;line-height:1;letter-spacing:0;transform:translateX(-4px)}");
  sendP(".tels span{text-align:right;font:9px/1 Consolas,'Lucida Console',monospace;letter-spacing:.04em;color:rgba(62,224,216,.55)}");
  sendP(".tels b{display:block;text-align:left;color:var(--cyan);font:700 11px/1 Consolas,'Lucida Console',monospace;letter-spacing:0;white-space:nowrap}");
  sendP(".diag{margin:0;font:700 19px/1.1 Consolas,'Lucida Console',monospace;letter-spacing:.08em;text-shadow:0 0 14px currentColor}");
  sendP(".w{position:relative;display:flex;align-items:center;gap:10px;background:linear-gradient(90deg,#10262a,#0c141c 42%);border:1px solid rgba(62,224,216,.28);padding:6px 8px;min-height:44px;cursor:pointer;width:max-content;clip-path:polygon(7px 0,100% 0,100% calc(100% - 5px),calc(100% - 7px) 100%,0 100%,0 5px)}");
  sendP(".w .id{font:12px Consolas,'Lucida Console',monospace;width:auto;color:var(--cyan);flex-shrink:0;text-shadow:0 0 6px rgba(62,224,216,.4)}");
  sendP(".w select{appearance:none;-webkit-appearance:none;flex:0 0 14.3ch;width:14.3ch;margin:0;padding:5px 18px 5px 6px;background:url(\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='6'%3E%3Cpath d='M1 1l4 4 4-4' fill='none' stroke='%233ee0d8' stroke-width='1.4'/%3E%3C/svg%3E\") no-repeat right 6px center,transparent;color:var(--ink);border:1px solid var(--rule);font:12px Consolas,'Lucida Console',monospace;letter-spacing:.04em;cursor:pointer;color-scheme:dark;outline:none}");
  sendP(".w select:hover,.w select:focus{color:#fff;border-color:var(--cyan)}");
  sendP(".w select option{background:#0b1116;color:var(--ink)}");
  sendP(".w select:focus-visible,label.btn:focus-within,a.btn:focus-visible,button:focus-visible,input:focus-visible{outline:1px solid var(--cyan);outline-offset:2px}");
  sendP(".left .w{border-left:3px solid var(--C);box-shadow:inset 3px 0 8px rgba(74,85,96,.25);clip-path:polygon(0 0,calc(100% - 7px) 0,100% 5px,100% 100%,7px 100%,0 calc(100% - 5px))}");
  sendP(".left .w-A{border-left-color:var(--A)}.left .w-B{border-left-color:var(--B)}.left .w-C{border-left-color:var(--C)}");
  sendP(".right .w{border-right:3px solid var(--C)}");
  sendP(".right .w-A{border-right-color:var(--A)}.right .w-B{border-right-color:var(--B)}.right .w-C{border-right-color:var(--C)}");
  sendP(".w-pending{border-color:var(--pend)!important;background:linear-gradient(90deg,#372913,#1a140c 50%)!important;animation:webPulse 1.5s infinite ease-in-out}");
  sendP("@keyframes webPulse{0%{opacity:.72}50%{opacity:1}100%{opacity:.72}}");
  sendP("@media(prefers-reduced-motion:reduce){.w-pending,.ticks,.chev{animation:none}.chev{opacity:.72}}");
  sendP(".sync{font:9px Consolas,monospace;color:var(--pend);letter-spacing:.08em}");
  sendP(".actions{display:flex;flex-wrap:wrap;gap:8px;margin-top:16px;align-items:center}");
  sendP("a.btn,label.btn,button.btn{position:relative;display:inline-block;padding:8px 14px;background:var(--glass);border:1px solid var(--rule);color:var(--ink);text-decoration:none;font:inherit;font-size:11px;letter-spacing:.1em;cursor:pointer;clip-path:polygon(8px 0,100% 0,calc(100% - 8px) 100%,0 100%)}");
  sendP("button.btn{-webkit-appearance:none;appearance:none}");
  sendP("a.btn:hover,label.btn:hover,button.btn:hover{border-color:var(--cyan);color:#fff;box-shadow:0 0 12px rgba(62,224,216,.25)}");
  sendP("a.btn:active,label.btn:active,button.btn:active{border-color:var(--rule);border-top-color:var(--ink);border-bottom-color:var(--ink);box-shadow:0 -6px 14px rgba(228,238,240,.35),0 6px 14px rgba(228,238,240,.35)}");
  sendP("label.btn input{position:absolute;opacity:0;pointer-events:none}");
  sendP("label.btn.on{background:rgba(212,238,242,.28);border-color:rgba(212,238,242,.5);color:#fff;box-shadow:inset 0 0 14px rgba(212,238,242,.18)}");
  sendP(".fork{margin-left:auto;display:inline-flex;align-items:center;gap:8px}");
  sendP("button.apply{position:relative;display:block;width:100%;margin-top:16px;padding:14px 12px;border:0;cursor:pointer;color:#fff;font:700 13px Consolas,monospace;letter-spacing:.26em;text-transform:uppercase;text-shadow:0 0 8px #fff,0 0 18px rgba(62,224,216,.75);background:transparent;overflow:visible}");
  sendP("button.apply span{position:relative;z-index:1}");
  sendP("button.apply:after{content:'';position:absolute;inset:0;pointer-events:none;background:rgba(62,224,216,.14);clip-path:polygon(14px 0,calc(100% - 14px) 0,100% 14px,100% calc(100% - 14px),calc(100% - 14px) 100%,14px 100%,0 calc(100% - 14px),0 14px)}");
  sendP("button.apply:before{content:'';position:absolute;inset:0;pointer-events:none;background:linear-gradient(135deg,transparent 46%,#c8ffff 46%,#c8ffff 54%,transparent 54%) left top/14px 14px no-repeat,linear-gradient(225deg,transparent 46%,#c8ffff 46%,#c8ffff 54%,transparent 54%) right top/14px 14px no-repeat,linear-gradient(45deg,transparent 46%,#c8ffff 46%,#c8ffff 54%,transparent 54%) left bottom/14px 14px no-repeat,linear-gradient(-45deg,transparent 46%,#c8ffff 46%,#c8ffff 54%,transparent 54%) right bottom/14px 14px no-repeat,linear-gradient(#c8ffff,#c8ffff) left 14px top 0/1.5% 2px no-repeat,linear-gradient(#c8ffff,#c8ffff) right 14px top 0/1.5% 2px no-repeat,linear-gradient(#c8ffff,#c8ffff) left 14px bottom 0/1.5% 2px no-repeat,linear-gradient(#c8ffff,#c8ffff) right 14px bottom 0/1.5% 2px no-repeat;filter:drop-shadow(0 0 2px #eaffff) drop-shadow(0 0 6px #3ee0d8) drop-shadow(0 0 14px rgba(62,224,216,.75))}");
  sendP("button.apply:hover:after{background:rgba(62,224,216,.22)}");
  sendP("button.apply:hover:before{filter:drop-shadow(0 0 3px #fff) drop-shadow(0 0 8px #3ee0d8) drop-shadow(0 0 18px rgba(62,224,216,.9))}");
  sendP("button.apply:active:before{filter:drop-shadow(0 0 4px #3ee0d8)}");
  sendP(".set{margin-top:22px;padding-top:14px;border-top:1px solid var(--rule)}");
  sendP(".set summary{position:relative;cursor:pointer;list-style:none;font-size:13px;letter-spacing:.14em;text-transform:uppercase;color:var(--ink);font-weight:600;display:flex;align-items:center;justify-content:flex-start;gap:12px;user-select:none;padding:6px 8px 6px 12px;border:1px solid var(--rule);background:linear-gradient(90deg,rgba(212,238,242,.1),transparent 55%)}");
  sendP(".set summary::-webkit-details-marker,.set summary::-moz-list-bullet{display:none}");
  sendP(".set summary::marker{content:''}");
  sendP(".set summary:hover{color:#fff;border-color:var(--amber)}");
  sendP(".set summary::after{content:'';position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);width:16px;height:16px;background:linear-gradient(#06080b,#06080b) center/10px 3.5px no-repeat,linear-gradient(#06080b,#06080b) center/3.5px 10px no-repeat,var(--amber);box-shadow:0 0 8px rgba(212,238,242,.4)}");
  sendP(".set[open] summary::after{background:linear-gradient(#06080b,#06080b) center/10px 3.5px no-repeat,var(--amber)}");
  sendP(".set[open] summary{margin-bottom:8px;border-color:var(--amber)}");
  sendP(".set label{display:block;font-size:10px;letter-spacing:.12em;color:var(--mute);margin-top:10px}");
  sendP(".set input[type=text],.set input[type=password]{width:100%;margin-top:4px;padding:8px;border:1px solid var(--rule);background:var(--glass);color:var(--ink)}");
  sendP(".set input[type=range]{width:100%;margin:8px 0;-webkit-appearance:none;appearance:none;height:6px;background:rgba(62,224,216,.18);border-radius:3px;outline:none;accent-color:var(--cyan)}");
  sendP(".set input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:16px;height:16px;border-radius:50%;background:var(--cyan);box-shadow:0 0 10px var(--cyan);cursor:pointer;border:0}");
  sendP(".set input[type=range]::-moz-range-thumb{width:16px;height:16px;border-radius:50%;background:var(--cyan);box-shadow:0 0 10px var(--cyan);cursor:pointer;border:0}");
  sendP(".set input[type=range]::-moz-range-track{height:6px;background:rgba(62,224,216,.18);border:0;border-radius:3px}");
  sendP("button.save{margin-top:12px;padding:8px 14px;background:transparent;border:1px solid var(--ink);color:var(--ink);font-size:11px;letter-spacing:.1em;cursor:pointer;clip-path:polygon(8px 0,100% 0,calc(100% - 8px) 100%,0 100%)}");
  sendP("button.save:hover{border-color:var(--cyan)}");
  sendP(".foot{margin:14px 0 0;font-size:9px;letter-spacing:.18em;color:var(--mute)}");
  sendP(".gate{position:relative;max-width:380px;margin:16vh auto 0;background:linear-gradient(165deg,rgba(16,24,32,.97),#07090c);border:1px solid var(--rule);padding:28px 24px;text-align:center;box-shadow:0 0 40px rgba(62,224,216,.08)}");
  sendP(".gate:before,.gate:after{content:'';position:absolute;width:14px;height:14px;pointer-events:none}");
  sendP(".gate:before{top:-1px;left:-1px;border-top:2px solid var(--cyan);border-left:2px solid var(--cyan)}");
  sendP(".gate:after{right:-1px;bottom:-1px;border-right:2px solid var(--amber);border-bottom:2px solid var(--amber)}");
  sendP(".gate svg{display:block;margin:0 auto 12px;width:120px;height:48px}");
  sendP(".gate p{margin:6px 0 16px;color:var(--mute);font-size:12px;letter-spacing:.04em}");
  sendP(".gate input{width:100%;margin:0 0 10px;padding:9px;border:1px solid var(--rule);background:var(--glass);color:var(--ink);text-align:center;letter-spacing:.2em}");
  sendP(".gate button{width:100%;padding:10px;border:0;background:var(--amber);color:#06080b;font-weight:700;letter-spacing:.16em;text-transform:uppercase;cursor:pointer;clip-path:polygon(10px 0,100% 0,calc(100% - 10px) 100%,0 100%)}");
  sendP("@media(max-width:620px){.antenna{grid-template-columns:1fr;gap:8px}.hub{order:-1;margin-bottom:8px;width:100%;max-width:280px;height:auto;aspect-ratio:1}.schem{display:none}.rig{padding:8px 0 4px}.mast{flex-direction:column;align-items:flex-start}.call{text-align:left}.fork{margin-left:0}.mast:before{display:none}}");
  sendP("</style></head><body>");
    
  if (!loggedIn) {
    sendP("<div class='gate'><p class='tag'>/// ACCESS LOCK</p><svg viewBox='0 0 120 40' aria-hidden='true'><path d='M8 34 L60 6 L112 34' fill='none' stroke='#3ee0d8' stroke-width='1.6'/><circle cx='60' cy='6' r='3.2' fill='#e4eef0' stroke='#d4eef2'/></svg>");
    sendP("<h1>Stožiar</h1><p>Zadajte PIN na ovládanie vodičov.</p>");
    sendP("<form action='/login' method='POST'>");
    sendP("<input type='password' name='webpin' placeholder='PIN' autocomplete='current-password' maxlength='4' inputmode='numeric'>");
    sendP("<button type='submit'>Otvoriť</button></form></div></body></html>");
    server.sendContent("", 0);
    return;
  }

  sendP("<div class='deck'><div class='mast'><div><p class='tag'>/// RF FEED ARRAY · SECTOR A</p><div class='brand'><h1>Stožiar</h1><div class='hudrow'><div><span>LINK</span><b>RS485</b></div><div><span>ARRAY</span><b>8-WIRE</b></div><div><span>FEED</span><b>INV-V</b></div><div><span>NODE</span><b>#");
  sendS(String(clientID));
  sendP("</b></div></div></div></div><div class='call'><b>");
  sendS(WiFi.localIP().toString());
  sendP("</b>NODE #");
  sendS(String(clientID));
  sendP(" · RS485</div></div>");
  if (!pca9685Present) sendP("<p class='warn'>LED modul chýba. Relé na stožiari idú ovládať aj tak.</p>");
  if (server.hasArg("busy")) sendP("<p class='warn'>Stožiar práve odpovedá. Skúste znova o chvíľu.</p>");
  if (server.hasArg("pinerr")) sendP("<p class='warn'>PIN musí mať presne 4 znaky. Zmena PIN sa neuložila.</p>");

  sendP("<form id='p-off' action='/profile' method='POST' hidden><input type='hidden' name='type' value='clear_all'></form>");
  sendP("<form id='p-left' action='/profile' method='POST' hidden><input type='hidden' name='type' value='beam_left'></form>");
  sendP("<form id='p-right' action='/profile' method='POST' hidden><input type='hidden' name='type' value='beam_right'></form>");
  
  sendP("<form action='/set' method='GET'>");

  String t1Display, t2Display, anomalyDisplay, anomalyColor;
  hubFields(anomalyDisplay, anomalyColor, t1Display, t2Display);

  sendP("<div class='rig'><svg class='schem' aria-hidden='true'></svg>");
  sendP("<div class='antenna'><div class='col left'>");
  sendP("<div class='tip'>");
  sendS(wireSelectHtml(4, "L1"));
  sendP("</div><div class='pair'>");
  sendS(wireSelectHtml(5, "L2"));
  sendS(wireSelectHtml(6, "L3"));
  sendP("</div><div class='tip'>");
  sendS(wireSelectHtml(7, "L4"));
  sendP("</div></div>");

  sendP("<div class='hub' style='--feed:");
  sendS(anomalyColor);
  sendP("'><div class='core'><svg class='cradle' viewBox='0 0 620 530' aria-hidden='true'><defs><filter id='glow' x='-120%' y='-120%' width='340%' height='340%'><feGaussianBlur stdDeviation='1.1' result='b'/><feMerge><feMergeNode in='b'/><feMergeNode in='SourceGraphic'/></feMerge></filter></defs>");
  sendP("<path fill='var(--plate)' fill-rule='evenodd' d='M 61.67,25.49 L 559.15,26.72 L 591.63,82.23 L 342.6,470 L 276.6,470 L 27.55,82.23 Z M 72.35,44.43 L 49.85,81.85 L 289.44,451 L 329.74,451 L 569.42,81.71 L 548.29,45.61 Z'/>");
  sendP("<path fill='none' stroke='var(--cyan)' stroke-width='1' opacity='.35' d='M 61.67,25.49 L 559.15,26.72 L 591.63,82.23 L 342.6,470 L 276.6,470 L 27.55,82.23 Z'/>");
  sendP("<path fill='var(--ink)' d='M 134.18,25.67 L 61.67,25.49 L 27.55,82.23 L 66.74,143.24 L 70.74,138.98 L 34.23,82.13 L 64.87,31.17 L 134.17,31.34 Z'/>");
  sendP("<path fill='var(--plate)' d='M 92.01,79.36 L 527.17,79.36 L 309.59,418.21 Z'/>");
  sendP("<path fill='var(--cyan)' opacity='.42' d='M 138.49,50.09 L 75.46,49.94 L 56.33,81.74 L 292.43,445.50 L 326.75,445.50 L 361.14,392.52 L 356.39,389.43 L 323.68,439.83 L 295.50,439.83 L 63.02,81.62 L 78.66,55.61 L 138.47,55.76 Z'/>");
  sendP("<path fill='var(--cyan)' opacity='.42' d='M 148,50.09 L 545.42,50.60 L 563.55,81.57 L 366.32,384.54 L 361.57,381.45 L 361.01,366.80 L 390.41,321.50 L 399.45,317.95 L 553.21,81.34 L 540.37,59.39 L 221.90,58.55 L 214,64.2 L 160,64.2 L 148,55.76 Z'/>");
  sendP("<polygon filter='url(#glow)' fill='none' stroke='var(--ink)' stroke-width='2.4' points='492.53,75.02 535.53,75.02 514.03,108.50'/>");
  sendP("<g filter='url(#glow)' fill='none' stroke='var(--ink)' stroke-width='3' stroke-linecap='round' stroke-linejoin='round'><path d='M 87.97,81.95 L 92.21,88.55 L 96.44,95.15'/></g>");
  sendP("<g filter='url(#glow)'><circle cx='108.84' cy='114.61' r='1.7' fill='var(--ink)'/><circle cx='112.79' cy='120.69' r='1.5' fill='var(--ink)'/><circle cx='117.33' cy='127.69' r='1.5' fill='var(--cyan)' opacity='.42'/><circle cx='121.65' cy='134.33' r='1.4' fill='var(--cyan)' opacity='.42'/><circle cx='127.55' cy='143.43' r='1.6' fill='var(--ink)'/><circle cx='254.90' cy='339.64' r='1.6' fill='var(--ink)'/><circle cx='258.84' cy='345.72' r='1.5' fill='var(--ink)'/><circle cx='275.23' cy='370.97' r='1.6' fill='var(--cyan)' opacity='.42'/><circle cx='279.53' cy='377.62' r='1.5' fill='var(--cyan)' opacity='.42'/><circle cx='283.47' cy='383.71' r='1.5' fill='var(--ink)'/></g></svg>");
  sendP("<span class='tick-clip' aria-hidden='true'><i class='ticks'></i></span>");
  sendP("<div class='feed' id='feed'><div class='mode' style='color:");
  sendS(anomalyColor);
  sendP("'>");
  sendS(htmlEscape(currentMode));
  sendP("</div><div class='diag' style='color:");
  sendS(anomalyColor);
  sendP("'><b id='web-status' data-current='");
  sendS(htmlEscape(anomalyDisplay));
  sendP("'>");
  sendS(htmlEscape(anomalyDisplay));
  sendP("</b></div><div class='tels'><span>RELAY</span><b id='t2'>");
  sendS(htmlEscape(t2Display));
  sendP("</b><span>MCU</span><b id='t1'>");
  sendS(htmlEscape(t1Display));
  sendP("</b></div></div></div></div>");

  sendP("<div class='col right'>");
  sendP("<div class='tip'>");
  sendS(wireSelectHtml(0, "R1"));
  sendP("</div><div class='pair'>");
  sendS(wireSelectHtml(1, "R2"));
  sendS(wireSelectHtml(2, "R3"));
  sendP("</div><div class='tip'>");
  sendS(wireSelectHtml(3, "R4"));
  sendP("</div></div></div></div>");

  sendP("<div class='actions'>");
  sendP("<button type='submit' form='p-off' class='btn'>Vypnúť vodiče</button>");
  sendP("<button type='submit' form='p-left' class='btn'>Lúč vľavo</button>");
  sendP("<button type='submit' form='p-right' class='btn'>Lúč vpravo</button>");
  sendP("<a href='/toggleStandby' class='btn btn-standby'>Stand-by</a>");
  sendP("<span class='fork'><span class='k'>Výhybka</span>");
  sendP("<label class='btn ");
  sendS((currentMode == "PHONE") ? "on" : "");
  sendP("'><input type='radio' name='fork_mode' value='PHONE'");
  sendS((currentMode == "PHONE") ? " checked" : "");
  sendP(">PHONE</label>");
  sendP("<label class='btn ");
  sendS((currentMode == "RTTY") ? "on" : "");
  sendP("'><input type='radio' name='fork_mode' value='RTTY'");
  sendS((currentMode == "RTTY") ? " checked" : "");
  sendP(">RTTY</label></span></div>");
  sendP("<button type='submit' class='apply'><span>Poslať na stožiar</span></button></form>");

  int currentPercent = clampLedPercent(ledPercent);
  sendP("<details class='set'><summary>Nastavenia stanice</summary><form action='/updateConfig' method='POST'>");
  sendP("<label>PIN</label><input type='text' name='newpin' value='");
  sendS(htmlEscape(currentPin));
  sendP("' maxlength='4' inputmode='numeric' autocomplete='off'>");
  sendP("<label>Wi-Fi sieť</label><input type='text' name='w_ssid' value='");
  sendS(htmlEscape(wifi_ssid));
  sendP("'>");
  sendP("<label>Wi-Fi heslo</label><input type='password' name='w_pass' placeholder='Nechať bezo zmeny'>");
  sendP("<label>Jas LED (");
  sendS(String(currentPercent));
  sendP("%)</label>");
  sendP("<input type='range' name='led_bright' min='5' max='100' value='");
  sendS(String(currentPercent));
  sendP("'>");
  sendP("<button type='submit' class='save'>Uložiť nastavenia</button></form></details>");
  sendP("<p class='foot'>/// MAST NODE · INVERTED-V · HALF-DUPLEX RS485</p></div>");

  sendP("<script>");
  sendP("document.querySelectorAll('.fork input').forEach(function(r){r.addEventListener('change',function(){document.querySelectorAll('.fork label').forEach(function(l){l.classList.toggle('on',l.querySelector('input').checked)});var m=document.querySelector('.feed .mode');if(m)m.textContent=this.value;});});");
  sendP("let isUserInteracting=false,interactTimer=null;");
  sendP("function markInteract(){isUserInteracting=true;clearTimeout(interactTimer);interactTimer=setTimeout(function(){isUserInteracting=false},8000)}");
  sendP("document.querySelectorAll('select').forEach(function(el){el.addEventListener('click',markInteract)});");
  sendP("(function(){var svg=document.querySelector('svg.schem'),rig=document.querySelector('.rig');if(!svg||!rig)return;");
  sendP("function colr(v,p){return p?'#ff9800':v==='A'?'#ff3333':v==='B'?'#3366ff':'#d4eef2'}");
  sendP("function ring(el,side,y){var R=rig.getBoundingClientRect(),b=el.getBoundingClientRect(),sy=(y-(b.top-R.top))/b.height*530,x1,y1,x2,y2;");
  sendP("if(!side){if(sy<82.23){x1=61.67;y1=25.49;x2=27.55;y2=82.23}else{x1=27.55;y1=82.23;x2=276.6;y2=470}}");
  sendP("else{if(sy<82.23){x1=559.15;y1=26.72;x2=591.63;y2=82.23}else{x1=591.63;y1=82.23;x2=342.6;y2=470}}");
  sendP("var t=(sy-y1)/(y2-y1);if(t<0)t=0;if(t>1)t=1;return b.left-R.left+((x1+t*(x2-x1))/620)*b.width}");
  sendP("function tOf(a,b,c,d){var l=Math.sqrt((c-a)*(c-a)+(d-b)*(d-b));if(l<8)return 0;var n=Math.max(1,Math.floor(Math.max(0,l-7)/7.2)+1);return Math.max(0,n-1)*.13}");
  sendP("function L(a,b,c,d,k,live,k2,d0){var dx=c-a,dy=d-b,len=Math.sqrt(dx*dx+dy*dy);if(len<8)return '';d0=d0||0;");
  sendP("var ang=Math.atan2(dy,dx)*180/Math.PI,ux=dx/len,uy=dy/len,px=-uy,py=ux,gw=7,minP=7.2,inner=Math.max(0,len-gw);");
  sendP("var n=Math.max(1,Math.floor(inner/minP)+1),pitch=n>1?inner/(n-1):0,t0=n>1?0:(len-gw)/2,o=3.1,kP=k,kM=k2||k;");
  sendP("if(k2&&(b+py*o)>(b-py*o)){kP=k2;kM=k}");
  sendP("var g='<g>';");
  sendP("g+='<path d=\"M'+(a+px*o)+' '+(b+py*o)+' L'+(c+px*o)+' '+(d+py*o)+'\" fill=\"none\" stroke=\"'+kP+'\" stroke-width=\"1.35\" opacity=\".45\"/>';");
  sendP("g+='<path d=\"M'+(a-px*o)+' '+(b-py*o)+' L'+(c-px*o)+' '+(d-py*o)+'\" fill=\"none\" stroke=\"'+kM+'\" stroke-width=\"1.35\" opacity=\".45\"/>';");
  sendP("for(var i=0;i<n;i++){var t=t0+i*pitch,x=a+ux*t,y=b+uy*t,ck=k2?(i%2?k2:k):k,cls=(live&&ck!=='#d4eef2')?'chev':'chev still';");
  sendP("g+='<g class=\"'+cls+'\" style=\"color:'+ck+';animation-delay:'+(d0+i*.13).toFixed(2)+'s\" transform=\"translate('+x.toFixed(1)+' '+y.toFixed(1)+') rotate('+ang.toFixed(1)+')\" fill=\"currentColor\" stroke=\"none\">';");
  sendP("g+='<path d=\"M0 -2.55 L4 0 L0 2.55 L2.3 2.55 L6.3 0 L2.3 -2.55 Z\"/></g>'}return g+'</g>'}");
  sendP("function brk(x,y,dir,k){var s=dir*5;return '<g stroke=\"'+k+'\" fill=\"none\" stroke-linecap=\"round\"><path d=\"M'+x+' '+(y-6)+' L'+(x+s)+' '+(y+6)+'\" stroke-width=\"1.5\"/><path d=\"M'+(x-dir*4)+' '+(y-6)+' L'+(x+s-dir*4)+' '+(y+6)+'\" stroke-width=\"1.5\"/><path d=\"M'+(x+dir*8)+' '+y+' L'+(x+dir*20)+' '+y+'\" stroke-width=\"1.2\" stroke-opacity=\".5\" stroke-dasharray=\"4 3.5\"/></g>'}");
  sendP("function nodeMark(nx,ny,rr,hasA,hasB){var r0=rr*.42,rm=(rr+r0)/2,s='',id='nh'+Math.round(nx)+Math.round(ny),kA='#ff3333',kB='#3366ff',kC='#d4eef2',live=hasA||hasB;");
  sendP("function rail(r,k){return '<circle cx=\"'+nx+'\" cy=\"'+ny+'\" r=\"'+r+'\" fill=\"none\" stroke=\"'+k+'\" stroke-width=\"1.35\" opacity=\".45\"/>'}");
  sendP("s+='<circle cx=\"'+nx+'\" cy=\"'+ny+'\" r=\"'+(rr+1)+'\" fill=\"#0b1116\"/>';");
  sendP("s+='<defs><mask id=\"'+id+'\"><circle cx=\"'+nx+'\" cy=\"'+ny+'\" r=\"'+(rr-0.9)+'\" fill=\"#fff\"/>';");
  sendP("s+='<circle cx=\"'+nx+'\" cy=\"'+ny+'\" r=\"'+(r0+0.9)+'\" fill=\"#000\"/></mask></defs><g mask=\"url(#'+id+')\">';");
  sendP("for(var i=0;i<8;i++){var a=i*45-90,rad=a*Math.PI/180,k=(hasA&&hasB)?(i%2?kB:kA):(hasA?kA:hasB?kB:kC);");
  sendP("var cls=(live&&k!==kC)?'chev':'chev still',x=nx+rm*Math.cos(rad),y=ny+rm*Math.sin(rad);");
  sendP("s+='<g class=\"'+cls+'\" style=\"color:'+k+';animation-delay:'+(i*.13).toFixed(2)+'s\" transform=\"translate('+x.toFixed(1)+' '+y.toFixed(1)+') rotate('+(a+90).toFixed(1)+') translate(-3.15 0)\" fill=\"currentColor\" stroke=\"none\">';");
  sendP("s+='<path d=\"M0 -2.55 L4 0 L0 2.55 L2.3 2.55 L6.3 0 L2.3 -2.55 Z\"/></g>'}s+='</g>';");
  sendP("if(hasA&&hasB)s+=rail(rr,kA)+rail(r0,kB);else{var k=hasA?kA:hasB?kB:kC;s+=rail(rr,k)+rail(r0,k)}return s}");
  sendP("function drawSchem(){var R=rig.getBoundingClientRect();svg.setAttribute('viewBox','0 0 '+R.width+' '+R.height);var h='',feed=document.querySelector('.cradle');if(!feed||R.width<8){svg.innerHTML='';return}");
  sendP("function bx(b,yy,sr){var t=yy-(b.top-R.top),bh=b.height,o=3.1,ch=5,cw=7,d=bh-t-o,ins=d<ch?cw*(1-Math.max(0,d)/ch)+2:0;return sr?b.right-R.left-ins:b.left-R.left+ins}");
  sendP("[['.left',0],['.right',1]].forEach(function(sp){var col=document.querySelector('.col'+sp[0]);if(!col)return;var right=sp[1],wires=[].slice.call(col.querySelectorAll('.w')),hasA=0,hasB=0;");
  sendP("wires.forEach(function(w){var v=w.querySelector('select').value;if(v==='A')hasA=1;if(v==='B')hasB=1});");
  sendP("if(wires.length<4)return;");
  sendP("var b1=wires[0].getBoundingClientRect(),b2=wires[1].getBoundingClientRect(),b3=wires[2].getBoundingClientRect(),b4=wires[3].getBoundingClientRect();");
  sendP("var nx=(b2.right+b3.left)/2-R.left,ny=((b1.top+b1.bottom)+(b4.top+b4.bottom))/4-R.top,pitch=9,rr=pitch/2+3.1;");
  sendP("var out=Math.min(b2.height,b3.height)/2-4.1,edge=right?R.width+22:-22,far=right?2:1,yUp=ny-pitch/2,yLo=ny+pitch/2;");
  sendP("var xF=ring(feed,right,yUp),dFeed=tOf(xF,yUp,nx,yUp);if(hasA&&hasB)h+=L(xF,yUp,nx,yUp,'#ff3333',1,'#3366ff');else{var tk=hasA?'#ff3333':hasB?'#3366ff':'#d4eef2';h+=L(xF,yUp,nx,yUp,tk,tk!=='#d4eef2')}");
  sendP("wires.forEach(function(w,i){var v=w.querySelector('select').value,p=w.className.indexOf('pending')>=0,k=colr(v,p),live=p||v==='A'||v==='B';");
  sendP("var b=w.getBoundingClientRect(),cy=b.top-R.top+b.height/2;");
  sendP("if(i===0||i===3){var tip=i===0,yT=tip?b.bottom-R.top:b.top-R.top,xW=bx(b,cy,right),dIn=dFeed;");
  sendP("h+=L(nx,ny,nx,yT,k,live,0,dIn);h+=L(xW,cy,edge,cy,k,live,0,dIn+tOf(nx,ny,nx,yT));h+=brk(edge,cy,right?1:-1,k)}");
  sendP("else{var y=ny+(i===far?-out:out),yIn=i===far?yUp:yLo,xIn=bx(b,yIn,i===far?(right?0:1):(i===1)),dIn=dFeed;");
  sendP("h+=L(nx,yIn,xIn,yIn,k,live,0,dIn);h+=L(bx(b,y,right),y,edge,y,k,live,0,dIn+tOf(nx,yIn,xIn,yIn));h+=brk(edge,y,right?1:-1,k)}});");
  sendP("h+=nodeMark(nx,ny,rr,hasA,hasB)});");
  sendP("svg.innerHTML=h}window.drawSchem=drawSchem;");
  sendP("document.querySelectorAll('.w select').forEach(function(s){s.addEventListener('change',function(){markInteract();var w=s.parentNode;w.classList.remove('w-A','w-B','w-C','w-pending');w.classList.add('w-'+s.value);var sy=w.querySelector('.sync');if(sy)sy.remove();drawSchem()})});");
  sendP("window.addEventListener('resize',drawSchem);drawSchem()})();");
  sendP("var setPanel=document.querySelector('details.set');");
  sendP("setInterval(function(){if(isUserInteracting||(setPanel&&setPanel.open))return;");
  sendP("fetch('/api/status').then(function(r){if(!r.ok)throw 0;return r.json()}).then(function(d){");
  sendP("var el=document.getElementById('web-status');if(!el)return;");
  sendP("var s=d.s||'';");
  sendP("if(s==='STANDBY')s='SPÁNOK';if(s==='NONE'||s==='OK')s='OK';if(s==='OFFLINE')s='TIMEOUT';if(s==='WAITING')s='ČAKÁ SA';");
  sendP("if(s==='ČAKÁ SA')return;");
  sendP("var t1=document.getElementById('t1'),t2=document.getElementById('t2');");
  sendP("if(t1&&d.t1)t1.textContent=d.t1;if(t2&&d.t2)t2.textContent=d.t2;");
  sendP("var hub=document.querySelector('.hub'),mode=document.querySelector('.feed .mode'),diag=document.querySelector('.diag');");
  sendP("if(d.c){if(hub)hub.style.setProperty('--feed',d.c);if(mode)mode.style.color=d.c;if(diag)diag.style.color=d.c}");
  sendP("if(s!==el.getAttribute('data-current'))location.reload();}).catch(function(){})},5000);");
  sendP("</script></body></html>");
  server.sendContent("", 0);
}

#endif
