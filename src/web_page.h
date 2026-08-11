#ifndef WEB_PAGE_H
#define WEB_PAGE_H

const char INDEX_HTML[] = R"WEB(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WRENCH // DEDSEC</title>
<style>
:root{--g:#b4ff39}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;padding:14px;background:#000;color:var(--g);font-family:'Courier New',monospace;
background-image:repeating-linear-gradient(0deg,rgba(0,0,0,0) 0 3px,rgba(180,255,57,.05) 3px 4px)}
header{text-align:center}
img.logo{width:64%;max-width:280px;image-rendering:pixelated}
pre.ascii{display:none;font-size:10px;line-height:1;color:var(--g)}
h1{font-size:20px;letter-spacing:6px;margin:6px 0;animation:gl 3s infinite}
@keyframes gl{0%,92%{text-shadow:2px 0 #0ff,-2px 0 #f0f}94%{text-shadow:-3px 0 #0ff,3px 0 #f0f;transform:translateX(2px)}96%{text-shadow:2px 0 #f0f,-2px 0 #0ff;transform:translateX(-2px)}98%{text-shadow:none;transform:none}}
#face{font-size:44px;text-align:center;letter-spacing:8px;margin:10px 0;white-space:pre}
.bar{position:relative;height:10px;border:1px solid var(--g);margin:6px 0}
.bar i{display:block;height:100%;background:var(--g);width:0%}
.mark{position:absolute;top:-2px;width:2px;height:14px;z-index:2;pointer-events:none}
.mark.lo{background:#0f0}
.mark.hi{background:#f33}
.mark small{position:absolute;top:-12px;left:-8px;font-size:9px;white-space:nowrap;color:inherit}
#bat{font-size:12px;opacity:.8}
.grid{display:grid;grid-template-columns:repeat(6,1fr);gap:4px;margin:10px 0}
button{background:#000;color:var(--g);border:1px solid var(--g);font-family:inherit;font-size:14px;padding:9px 2px;white-space:pre}
button:active{background:var(--g);color:#000}
.modes{display:grid;grid-template-columns:repeat(4,1fr);gap:4px}
.modes button.on{background:var(--g);color:#000}
.sl{margin:10px 0;font-size:12px}
input[type=range]{width:100%;accent-color:var(--g);background:#000}
.big{width:100%;padding:12px;font-size:16px;margin:6px 0}
footer{font-size:10px;opacity:.5;text-align:center;margin-top:14px}
</style>
</head>
<body>
<header>
<img class="logo" src="data:image/png;base64,__LOGO_B64__" onerror="this.style.display='none';document.getElementById('ascii').style.display='block'">
<pre id="ascii">
   _____
  /     \
 | () () |
  \  ^  /
   |||||
</pre>
<h1>DEDSEC CONTROL</h1>
</header>

<div id="face">^ ^</div>
<div class="bar">
  <i id="vol"></i>
  <span class="mark lo" id="mkLo"><small>LOW</small></span>
  <span class="mark hi" id="mkHi"><small>HIGH</small></span>
</div>
<div id="bat">BAT: --%</div>

<div class="modes">
<button id="m0" onclick="cmd('m=0')">AUTO</button>
<button id="m1" onclick="cmd('m=1')">BTN</button>
<button id="m2" onclick="cmd('m=2')">MIC</button>
<button id="m3" onclick="cmd('m=3')">CAR</button>
</div>

<div class="grid" id="emo"></div>

<button class="big" onclick="cmd('flip=1')">FLIP SCREEN</button>
<button class="big" onclick="cmd('scare=1')">TEST SCARE</button>
<button class="big" onclick="cmd('calib=1')">CALIBRATE IMU</button>

<div class="sl">MIC SENS <span id="v_ms"></span><input type="range" min="10" max="150" id="ms" onchange="cfg()"></div>
<div class="sl">TH LOW <span id="v_tl"></span><input type="range" min="10" max="150" id="tl" onchange="cfg()"></div>
<div class="sl">TH HIGH <span id="v_th"></span><input type="range" min="50" max="600" id="th" onchange="cfg()"></div>
<div class="sl">SCARE % <span id="v_se"></span><input type="range" min="30" max="100" id="se" onchange="cfg()"></div>
<div class="sl">BRIGHT <span id="v_br"></span><input type="range" min="20" max="255" id="br" onchange="cfg()"></div>

<footer>WE ARE DEDSEC // FOLLOW THE WHITE RABBIT</footer>

<script>
const F=["X X","@ @","O o","# #","~ ^","* *","^ ^","\\ /","/ \\","= =","9 9","- -","> <","? ?","Z Z","; ;","! !"];
const g=id=>document.getElementById(id);
F.forEach((f,i)=>{const b=document.createElement('button');b.textContent=f;b.onclick=()=>cmd('e='+i);g('emo').appendChild(b);});
function cmd(q){fetch('/cmd?'+q)}
function cfg(){
  g('mkLo').style.left = (g('tl').value * 100 / 960) + '%';
  g('mkHi').style.left = (g('th').value * 100 / 960) + '%';
  fetch('/cmd?ms='+(g('ms').value/100)+'&tl='+g('tl').value+'&th='+g('th').value+'&se='+g('se').value+'&br='+g('br').value)
}
let first=true;
setInterval(async()=>{
 try{const j=await(await fetch('/state')).json();
 g('face').textContent=j.face;
 g('vol').style.width=j.vol+'%';
// маркеры порогов на полоске (thLow/thHigh в единицах volSmooth, шкала ~960)
g('mkLo').style.left = (j.tl * 100 / 960) + '%';
g('mkHi').style.left = (j.th * 100 / 960) + '%';
 g('bat').textContent='BAT: '+j.bat+'%  MODE: '+['AUTO','BTN','MIC','CAR'][j.mode];
 for(let i=0;i<4;i++)g('m'+i).className=(j.mode==i)?'on':'';
 if(first){
  first=false;
  g('ms').value=j.ms*100;
  g('tl').value=j.tl;
  g('th').value=j.th;
  g('se').value=j.se;
  g('br').value=j.br;
  g('mkLo').style.left = (j.tl * 100 / 960) + '%';
  g('mkHi').style.left = (j.th * 100 / 960) + '%';
 }
 g('v_ms').textContent=(g('ms').value/100).toFixed(2);
 g('v_tl').textContent=g('tl').value;g('v_th').textContent=g('th').value;
 g('v_se').textContent=g('se').value;g('v_br').textContent=g('br').value;
 }catch(e){}
},600);
</script>
</body>
</html>
)WEB";

#endif