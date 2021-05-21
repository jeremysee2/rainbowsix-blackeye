/*
 * simpleviewer and streamviewer
 */

const uint8_t index_simple_html[] = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title id="title">ESP32-CAM Simplified View</title>
    <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
    <link rel="icon" type="image/png" sizes="16x16" href="/favicon-16x16.png">
    <link rel="stylesheet" type="text/css" href="/style.css">
    <style>
    // style overrides here
    </style>
  </head>

  <body>
    <section class="main">
      <div id="logo">
        <label for="nav-toggle-cb" id="nav-toggle" style="float:left;" title="Settings">&#9776;&nbsp;</label>
        <button id="swap-viewer" style="float:left;" title="Swap to full feature viewer">Full</button>
        <button id="get-still" style="float:left;">Get Still</button>
        <button id="toggle-stream" style="float:left;" class="hidden">Start Stream</button>
        <div id="wait-settings" style="float:left;" class="loader" title="Waiting for camera settings to load"></div>
      </div>
      <div id="content">
        <div class="hidden" id="sidebar">
          <input type="checkbox" id="nav-toggle-cb">
            <nav id="menu" style="width:24em;">
              <div class="input-group hidden" id="lamp-group">
                <label for="lamp">Light</label>
                <div class="range-min">Off</div>
                <input type="range" id="lamp" min="0" max="100" value="0" class="action-setting">
                <div class="range-max">Full</div>
              </div>
              <div class="input-group" id="framesize-group">
                <label for="framesize">Resolution</label>
                <select id="framesize" class="action-setting">
                  <option value="10">UXGA(1600x1200)</option>
                  <option value="9">SXGA(1280x1024)</option>
                  <option value="8">XGA(1024x768)</option>
                  <option value="7">SVGA(800x600)</option>
                  <option value="6">VGA(640x480)</option>
                  <option value="5">CIF(400x296)</option>
                  <option value="4">QVGA(320x240)</option>
                  <option value="3">HQVGA(240x176)</option>
                  <option value="0">QQVGA(160x120)</option>
                </select>
              </div>
              <!-- Hide the next entries, they are present in the body so that we
                  can pass settings to/from them for use in the scripting, not for user setting -->
              <div id="rotate" class="action-setting hidden"></div>
              <div id="cam_name" class="action-setting hidden"></div>
              <div id="stream_url" class="action-setting hidden"></div>
              <div id="detect" class="action-setting hidden"></div> 
              <div id="recognize" class="action-setting hidden"></div>
            </nav>
        </div>
        <figure>
          <div id="stream-container" class="image-container hidden">
            <div class="close close-rot-none" id="close-stream">×</div>
            <img id="stream" src="">
          </div>
        </figure>
      </div>
    </section>
  </body>

  <script>
  document.addEventListener('DOMContentLoaded', function (event) {
    var baseHost = document.location.origin;
    var streamURL = 'Undefined';

    const settings = document.getElementById('sidebar')
    const waitSettings = document.getElementById('wait-settings')
    const lampGroup = document.getElementById('lamp-group')
    const rotate = document.getElementById('rotate')
    const view = document.getElementById('stream')
    const viewContainer = document.getElementById('stream-container')
    const stillButton = document.getElementById('get-still')
    const streamButton = document.getElementById('toggle-stream')
    const closeButton = document.getElementById('close-stream')
    const swapButton = document.getElementById('swap-viewer')

    const hide = el => {
      el.classList.add('hidden')
    }
    const show = el => {
      el.classList.remove('hidden')
    }

    const disable = el => {
      el.classList.add('disabled')
      el.disabled = true
    }

    const enable = el => {
      el.classList.remove('disabled')
      el.disabled = false
    }

    const updateValue = (el, value, updateRemote) => {
      updateRemote = updateRemote == null ? true : updateRemote
      let initialValue
      if (el.type === 'checkbox') {
        initialValue = el.checked
        value = !!value
        el.checked = value
      } else {
        initialValue = el.value
        el.value = value
      }

      if (updateRemote && initialValue !== value) {
        updateConfig(el);
      } else if(!updateRemote){
        if(el.id === "lamp"){
          if (value == -1) { 
            hide(lampGroup)
          } else {
            show(lampGroup)
          }
        } else if(el.id === "cam_name"){
          window.document.title = value;
          console.log('Name set to: ' + value);
        } else if(el.id === "code_ver"){
          console.log('Firmware Build: ' + value);
        } else if(el.id === "rotate"){
          rotate.value = value;
          applyRotation();
        } else if(el.id === "stream_url"){
          streamURL = value;
          streamButton.setAttribute("title", `Start the stream :: {streamURL}`);
          console.log('Stream URL set to:' + value);
        } 
      }
    }

    function updateConfig (el) {
      let value
      switch (el.type) {
        case 'checkbox':
          value = el.checked ? 1 : 0
          break
        case 'range':
        case 'select-one':
          value = el.value
          break
        case 'button':
        case 'submit':
          value = '1'
          break
        default:
          return
      }

      const query = `${baseHost}/control?var=${el.id}&val=${value}`

      fetch(query)
        .then(response => {
          console.log(`request to ${query} finished, status: ${response.status}`)
        })
    }

    document
      .querySelectorAll('.close')
      .forEach(el => {
        el.onclick = () => {
          hide(el.parentNode)
        }
      })

    // read initial values
    fetch(`${baseHost}/status`)
      .then(function (response) {
        return response.json()
      })
      .then(function (state) {
        document
          .querySelectorAll('.action-setting')
          .forEach(el => {
            updateValue(el, state[el.id], false)
          })
        hide(waitSettings);
        show(settings);
        show(streamButton);
        startStream();
      })

    // Put some helpful text on the 'Still' button
    stillButton.setAttribute("title", `Capture a still image :: ${baseHost}/capture`);

    const stopStream = () => {
      window.stop();
      streamButton.innerHTML = 'Start Stream';
          streamButton.setAttribute("title", `Start the stream :: ${streamURL}`);
      hide(viewContainer);
    }

    const startStream = () => {
      view.src = streamURL;
      view.scrollIntoView(false);
      streamButton.innerHTML = 'Stop Stream';
      streamButton.setAttribute("title", `Stop the stream`);
      show(viewContainer);
    }

    const applyRotation = () => {
      rot = rotate.value;
      if (rot == -90) {
        viewContainer.style.transform = `rotate(-90deg)  translate(-100%)`;
        closeButton.classList.remove('close-rot-none');
        closeButton.classList.remove('close-rot-right');
        closeButton.classList.add('close-rot-left');
      } else if (rot == 90) {
        viewContainer.style.transform = `rotate(90deg) translate(0, -100%)`;
        closeButton.classList.remove('close-rot-left');
        closeButton.classList.remove('close-rot-none');
        closeButton.classList.add('close-rot-right');
      } else {
        viewContainer.style.transform = `rotate(0deg)`;
        closeButton.classList.remove('close-rot-left');
        closeButton.classList.remove('close-rot-right');
        closeButton.classList.add('close-rot-none');
      }
       console.log('Rotation ' + rot + ' applied');
   }

    // Attach actions to controls

    stillButton.onclick = () => {
      stopStream();
      view.src = `${baseHost}/capture?_cb=${Date.now()}`;
      view.scrollIntoView(false);
      show(viewContainer);
    }

    closeButton.onclick = () => {
      stopStream();
      hide(viewContainer);
    }

    streamButton.onclick = () => {
      const streamEnabled = streamButton.innerHTML === 'Stop Stream'
      if (streamEnabled) {
        stopStream();
      } else {
        startStream();
      }
    }

    // Attach default on change action
    document
      .querySelectorAll('.action-setting')
      .forEach(el => {
        el.onchange = () => updateConfig(el)
      })

    // Custom actions
    // Detection and framesize
    rotate.onchange = () => {
      applyRotation();
      updateConfig(rotate);
    }

    framesize.onchange = () => {
      updateConfig(framesize)
      if (framesize.value > 5) {
        updateValue(detect, false)
        updateValue(recognize, false)
      }
    }

    swapButton.onclick = () => {
      window.open('/?view=full','_self');
    }

  })
  </script>
</html>)=====";

size_t index_simple_html_len = sizeof(index_simple_html)-1;

/* Stream Viewer */

const uint8_t streamviewer_html[] = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title id="title">ESP32-CAM StreamViewer</title>
    <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
    <link rel="icon" type="image/png" sizes="16x16" href="/favicon-16x16.png">
    <style>
      /* No stylesheet, define all style elements here */
      body {
        font-family: Arial,Helvetica,sans-serif;
        background: #181818;
        color: #EFEFEF;
        font-size: 16px;
        margin: 0px;
        overflow:hidden;
      }

      img {
        object-fit: contain;
        display: block;
        margin: 0px;
        padding: 0px;
        width: 100vw;
        height: 100vh;
      }

      .loader {
        border: 0.5em solid #f3f3f3;
        border-top: 0.5em solid #000000;
        border-radius: 50%;
        width: 1em;
        height: 1em;
        -webkit-animation: spin 2s linear infinite; /* Safari */
        animation: spin 2s linear infinite;
      }

      @-webkit-keyframes spin {   /* Safari */
        0% { -webkit-transform: rotate(0deg); }
        100% { -webkit-transform: rotate(360deg); }
      }

      @keyframes spin {
        0% { transform: rotate(0deg); }
        100% { transform: rotate(360deg); }
      }
    </style>
  </head>

  <body>
    <section class="main">
      <div id="wait-settings" style="float:left;" class="loader" title="Waiting for stream settings to load"></div>
      <div style="display: none;">
        <!-- Hide the next entries, they are present in the body so that we
             can pass settings to/from them for use in the scripting -->
        <div id="rotate" class="action-setting hidden">0</div>
        <div id="cam_name" class="action-setting hidden"></div>
        <div id="stream_url" class="action-setting hidden"></div>
      </div>
      <img id="stream" src="">
    </section>
  </body>

  <script>
  document.addEventListener('DOMContentLoaded', function (event) {
    var baseHost = document.location.origin;
    var streamURL = 'Undefined';

    const rotate = document.getElementById('rotate')
    const stream = document.getElementById('stream')
    const spinner = document.getElementById('wait-settings')

    const updateValue = (el, value, updateRemote) => {
      updateRemote = updateRemote == null ? true : updateRemote
      let initialValue
      if (el.type === 'checkbox') {
        initialValue = el.checked
        value = !!value
        el.checked = value
      } else {
        initialValue = el.value
        el.value = value
      }

      if (updateRemote && initialValue !== value) {
        updateConfig(el);
      } else if(!updateRemote){
        if(el.id === "cam_name"){
          window.document.title = value;
          stream.setAttribute("title", value + "\n(doubleclick for fullscreen)");
          console.log('Name set to: ' + value);
        } else if(el.id === "rotate"){
          rotate.value = value;
          console.log('Rotate recieved: ' + rotate.value);
        } else if(el.id === "stream_url"){
          streamURL = value;
          console.log('Stream URL set to:' + value);
        } 
      }
    }

    // read initial values
    fetch(`${baseHost}/info`)
      .then(function (response) {
        return response.json()
      })
      .then(function (state) {
        document
          .querySelectorAll('.action-setting')
          .forEach(el => {
            updateValue(el, state[el.id], false)
          })
        spinner.style.display = `none`;
        applyRotation();
        startStream();
      })

    const startStream = () => {
      stream.src = streamURL;
      stream.style.display = `block`;
    }

    const applyRotation = () => {
      rot = rotate.value;
      if (rot == -90) {
        stream.style.transform = `rotate(-90deg)`;
      } else if (rot == 90) {
        stream.style.transform = `rotate(90deg)`;
      }
      console.log('Rotation ' + rot + ' applied');
    }

    stream.ondblclick = () => {
      if (stream.requestFullscreen) {
        stream.requestFullscreen();
      } else if (stream.mozRequestFullScreen) { /* Firefox */
        stream.mozRequestFullScreen();
      } else if (stream.webkitRequestFullscreen) { /* Chrome, Safari and Opera */
        stream.webkitRequestFullscreen();
      } else if (stream.msRequestFullscreen) { /* IE/Edge */
        stream.msRequestFullscreen();
      }
    }
  })
  </script>
</html>)=====";

size_t streamviewer_html_len = sizeof(streamviewer_html)-1;

/* Captive Portal page 
   we replace the <> delimited strings with correct values as it is served */

const std::string portal_html = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title id="title"><CAMNAME> - portal</title>
    <link rel="icon" type="image/png" sizes="32x32" href="<APPURL>favicon-32x32.png">
    <link rel="icon" type="image/png" sizes="16x16" href="<APPURL>favicon-16x16.png">
    <link rel="stylesheet" type="text/css" href="<APPURL>style.css">
  </head>
  <body style="text-align: center;">
    <img src="<APPURL>logo.svg" style="position: relative; float: right;">
    <h1><CAMNAME> - access portal</h1>
    <div class="input-group" style="margin: auto; width: max-content;">
      <a href="<APPURL>?view=simple" title="Click here for a simple view with minimum control" style="text-decoration: none;" target="_blank">
      <button>Simple Viewer</button></a>
      <a href="<APPURL>?view=full" title="Click here for the main camera page with full controls" style="text-decoration: none;" target="_blank">
      <button>Full Viewer</button></a>
      <a href="<STREAMURL>view" title="Click here for the dedicated stream viewer" style="text-decoration: none;" target="_blank">
      <button>Stream Viewer</button></a>
    </div>
    <hr>
    <a href="<APPURL>dump" title="Information dump page" target="_blank">Camera Details</a><br>
  </body>
</html>)=====";

/* Error page 
   we replace the <> delimited strings with correct values as it is served */

const std::string error_html = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title id="title"><CAMNAME> - Error</title>
    <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
    <link rel="ico\" type="image/png" sizes="16x16" href="/favicon-16x16.png">
    <link rel="stylesheet" type="text/css" href="<APPURL>style.css">
  </head>
  <body style="text-align: center;">
    <img src="<APPURL>logo.svg" style="position: relative; float: right;">
    <h1><CAMNAME></h1>
    <ERRORTEXT>
  </body>
  <script>
    setTimeout(function(){
      location.replace(document.URL);
    }, 60000);
  </script>
</html>)=====";

/* Blackeye control page 
   we replace the <> delimited strings with correct values as it is served */

  const uint8_t index_blackeye_html[] = R"=====(
<!--This code is based on a project by Seb Lee-Delisle: http://seb.ly/2011/04/multi-touch-game-controller-in-javascripthtml5-for-ipad/-->
<!--And Max K from CoreTech: https://github.com/CoretechR/ESP32-WiFi-Robot-->>
<!doctype html>
<html lang=en style="padding-bottom:80px">
  <head>
    <meta charset=utf-8>
    <meta name="viewport" content="width=device-width, height=device-height, initial-scale=1.0, maximum-scale=1.0, user-scalable=0" />
    <meta name="mobile-web-app-capable" content="yes">
    <title>Black Eye Camera</title>
    <style type="text/css"> 
* {
  -webkit-touch-callout: none; /* prevent callout to copy image, etc when tap to hold */
  -webkit-text-size-adjust: none; /* prevent webkit from resizing text to fit */
  /* make transparent link selection, adjust last value opacity 0 to 1.0 */
  -webkit-tap-highlight-color: rgba(0,0,0,0); 
  -webkit-user-select: none; /* prevent copy paste, to allow, change 'none' to 'text' */
  -webkit-tap-highlight-color: rgba(0,0,0,0); 
}
body {
  background-color: #000000;
  margin: 0px;
}
canvas {
  display:block; 
  position:absolute; 
  z-index: 1;
}
.container {
  width:auto;
  text-align:center;
  background-color:#ff0000;
}

input[type="checkbox"]:checked + label {
    background: gray;
}

canvas#stream{
  //display: block;
  margin: 0px;
  //width: 100%;
  height: auto;
  //position:absolute;
  //top:6vh;right:0;left:0;
  margin:auto;
  background:rgba(0,255,0,0.5);
  z-index:1;
}
    </style>
    <link href="https://fonts.googleapis.com/css?family=Roboto" rel="stylesheet">
  </head>
  <!--
    <ul id="messages"></ul>
    
    <center>
    <canvas id="stream" width="800" height="600" style="z-index: -1;"></canvas>
    </center>
    -->
  <body scroll="no" style="overflow: hidden; height:100vh;">
    
  <script>

host = window.location.hostname;
if(!host) host = '192.168.4.1'; //For debugging only 

function toStatus(txt) {document.getElementById('status').innerHTML=txt; }
function toReply(txt) {document.getElementById('reply').innerHTML=txt; }

var Vector2 = function (x,y) {
  this.x = x || 0;
  this.y = y || 0;
};

Vector2.prototype = {

  reset: function ( x, y ) {
  
    this.x = x;
    this.y = y;
    return this;
  },
  
  copyFrom : function (v) {
    this.x = v.x;
    this.y = v.y;
  },
  
  plusEq : function (v) {
    this.x+=v.x;
    this.y+=v.y;
    return this;
  },
  
  minusEq : function (v) {
    this.x-=v.x;
    this.y-=v.y;
    return this; 
  },
  
  equals : function (v) {
    return((this.x==v.x)&&(this.y==v.x));
  }

};

var canvas,
  scanvas,
  c, // c is the canvas' context 2D
  container, 
  halfWidth, 
  halfHeight,
  leftTouchID = -1, 
  leftTouchPos = new Vector2(0,0),
  leftTouchStartPos = new Vector2(0,0),
  leftVector = new Vector2(0,0);

var temperature;
var sendFlag = false;

setupCanvas();

var mouseX, 
  mouseY,
  mouseDown = false,
  touches = []; // array of touch vectors;
  
var headlight = 0;

setInterval(draw, 1000/30); // draw app at 30fps

setInterval(sendControls, 1000/20); // send control input at 20fps

canvas.addEventListener( 'touchstart', onTouchStart, false );
canvas.addEventListener( 'touchmove', onTouchMove, false );
canvas.addEventListener( 'touchend', onTouchEnd, false );
window.onorientationchange = resetCanvas;  
window.onresize = resetCanvas;  

canvas.addEventListener( 'mousemove', onMouseMove, false );
canvas.addEventListener( 'mousedown', onMouseDown, false );
canvas.addEventListener( 'mouseup', onMouseUp, false );


function resetCanvas (e) {
  // resize the canvas - but remember - this clears the canvas too. 
  canvas.width = window.innerWidth; 
  canvas.height = window.innerHeight;
 
  //halfWidth = canvas.width/2; 
  halfWidth = canvas.width;

  halfHeight = canvas.height/2;
 
  //make sure we scroll to the top left. 
  window.scrollTo(0,0); 
}

var rawLeft, rawRight, MaxJoy = 255, MinJoy = -255, MaxValue = 255,
  MinValue = -255, RawLeft, RawRight, ValLeft, ValRight;
var leftMot = 0, rightMot = 0;

function Remap(value, from1, to1, from2, to2){
  return (value - from1) / (to1 - from1) * (to2 - from2) + from2;
}

//source: http://www.dyadica.co.uk/basic-differential-aka-tank-drive/
function tankDrive(x, y){
  
  var z = Math.sqrt(x * x + y * y);
  var rad = Math.acos(Math.abs(x) / z);
  
  if (isNaN(rad)) rad = 0;
  var angle = rad * 180 / Math.PI;
  var tcoeff = -1 + (angle / 90) * 2;
  var turn = tcoeff * Math.abs(Math.abs(y) - Math.abs(x));
  
  turn = Math.round(turn * 100) / 100;
  var move = Math.max(Math.abs(y), Math.abs(x));
  
  if ((x >= 0 && y >= 0) || (x < 0 && y < 0)){
    rawLeft = move;
    rawRight = turn;
  }else{
    rawRight = move;
    rawLeft = turn;
  }

  if (y < 0){
    rawLeft = 0 - rawLeft;
    rawRight = 0 - rawRight;
  }
  
  RawLeft = rawLeft;
  RawRight = rawRight;
  
  leftMot = Remap(rawLeft, MinJoy, MaxJoy, MinValue, MaxValue);
  rightMot = Remap(rawRight, MinJoy, MaxJoy, MinValue, MaxValue);
}

function draw() {
  
  c.clearRect(0,0,canvas.width, canvas.height); 

  //if touch
  for(var i=0; i<touches.length; i++) {
    
    var touch = touches[i]; 
    
    if(touch.identifier == leftTouchID){
      c.beginPath(); 
      c.strokeStyle = "white"; 
      c.lineWidth = 6; 
      c.arc(leftTouchStartPos.x, leftTouchStartPos.y, 40,0,Math.PI*2,true); 
      c.stroke();
      c.beginPath(); 
      c.strokeStyle = "white"; 
      c.lineWidth = 2; 
      c.arc(leftTouchStartPos.x, leftTouchStartPos.y, 60,0,Math.PI*2,true); 
      c.stroke();
      c.beginPath(); 
      c.strokeStyle = "white"; 
      c.arc(leftTouchPos.x, leftTouchPos.y, 40, 0,Math.PI*2, true); 
      c.stroke(); 
      
    } else {
      
      c.beginPath(); 
      c.fillStyle = "white";
      //c.fillText("touch id : "+touch.identifier+" x:"+touch.clientX+" y:"+touch.clientY, touch.clientX+30, touch.clientY-30); 
  
      c.beginPath(); 
      c.strokeStyle = "red";
      c.lineWidth = "6";
      c.arc(touch.clientX, touch.clientY, 40, 0, Math.PI*2, true); 
      c.stroke();
    }
  }

  //if not touch   
  if(mouseDown){
  
    c.beginPath(); 
    c.strokeStyle = "white"; 
    c.lineWidth = 6; 
    c.arc(leftTouchStartPos.x, leftTouchStartPos.y, 40,0,Math.PI*2,true); 
    c.stroke();
    c.beginPath(); 
    c.strokeStyle = "white"; 
    c.lineWidth = 2; 
    c.arc(leftTouchStartPos.x, leftTouchStartPos.y, 60,0,Math.PI*2,true); 
    c.stroke();
    c.beginPath(); 
    c.strokeStyle = "white"; 
    c.arc(leftTouchPos.x, leftTouchPos.y, 40, 0,Math.PI*2, true); 
    c.stroke(); 
        
    c.fillStyle  = "white"; 
    //c.fillText("mouse : "+mouseX+", "+mouseY, mouseX, mouseY); 
    c.beginPath(); 
    c.strokeStyle = "white";
    c.lineWidth = "6";
    c.arc(mouseX, mouseY, 40, 0, Math.PI*2, true); 
    c.stroke();
  }

  var scanvas = document.getElementById('stream');
  var ctx = scanvas.getContext('2d');
  var img = document.getElementById('streamImg');
  ctx.drawImage(img,0,0);
 
}

window.addEventListener('resize', resizeStream, false);

function resizeStream() {
   aspectRatio = 800/600;
   maxHeight = window.innerHeight - document.getElementById('navTop').offsetHeight - document.getElementById('navBottom').offsetHeight;
   if(window.innerWidth*aspectRatio <= maxHeight){
     document.getElementById('stream').style.height = window.innerWidth*aspectRatio + "px";
     document.getElementById('stream').style.width = window.innerWidth + "px";
   }
   else {
     document.getElementById('stream').style.height = maxHeight + "px";
     document.getElementById('stream').style.width = maxHeight/aspectRatio + "px";
   }
 }


/* 
* Touch event
*/  

function onTouchStart(e) {
  for(var i = 0; i<e.changedTouches.length; i++){
    var touch =e.changedTouches[i];
    //console.log(leftTouchID + " "
    if((leftTouchID<0) && (touch.clientX<halfWidth)){
	
      leftTouchID = touch.identifier;
      leftTouchStartPos.reset(touch.clientX, touch.clientY);
      leftTouchPos.copyFrom(leftTouchStartPos);
      leftVector.reset(0,0);
      continue;
    } else{
	
      makeBullet();
    }
  }
  touches = e.touches;
}

function onMouseDown(event) {
  leftTouchStartPos.reset(event.offsetX, event.offsetY);
  leftTouchPos.copyFrom(leftTouchStartPos);
  leftVector.reset(0,0);
  mouseDown = true;
}
 
function onTouchMove(e) {
  // Prevent the browser from doing its default thing (scroll, zoom)
  e.preventDefault();
  for(var i = 0; i<e.changedTouches.length; i++){
    var touch =e.changedTouches[i];
    if(leftTouchID == touch.identifier){
      leftTouchPos.reset(touch.clientX, touch.clientY);
      leftVector.copyFrom(leftTouchPos);
      leftVector.minusEq(leftTouchStartPos);
      sendFlag = true;
      break;
    }
  }
  touches = e.touches;
}

function onMouseMove(event){
  mouseX = event.offsetX;
  mouseY = event.offsetY;
  if(mouseDown){
    leftTouchPos.reset(event.offsetX, event.offsetY);
    leftVector.copyFrom(leftTouchPos);
    leftVector.minusEq(leftTouchStartPos);
    sendFlag = true;
  }
}
 
function onTouchEnd(e){
  touches = e.touches;
  for(var i = 0; i<e.changedTouches.length; i++){
    var touch =e.changedTouches[i];
    if(leftTouchID == touch.identifier){
      leftTouchID = -1;
      leftVector.reset(0,0);
      leftMot = rightMot = 0;
      sendFlag = true;
      break;
    }
  }
}

function onMouseUp(event) { 
  leftVector.reset(0,0);
  leftMot = rightMot = 0;
  mouseDown = false;
  sendFlag = true;
}

/*
Source for keyboard detection: Braden Best:
https://stackoverflow.com/questions/5203407/how-to-detect-if-multiple-keys-are-pressed-at-once-using-javascript
*/ 
var map = {};
onkeydown = onkeyup = function(e){
  e = e || event; // to deal with IE
  map[e.keyCode] = e.type == 'keydown';
  
  if(map[38]){ // ArrowUp
    leftVector.y = -55;
  }
  if(map[40]){ // ArrowDown
    leftVector.y = 55;
  }
  if(map[37]){ // ArrowLeft
    leftVector.x = -55;
  }
  if(map[39]){ // ArrowRight
    leftVector.x = 55;
  }
  
  if(!map[38] && !map[40]){ // ArrowUp/Down is not pressed
    leftVector.y = 0;
  }
  if(!map[37] && !map[39]){ // ArrowLeft/Right is not pressed
    leftVector.x = 0;
  }
  if(leftVector.y == 0 && leftVector.x == 0) leftMot = rightMot = 0;
  sendFlag = true;
}

function setupCanvas() {
  canvas = document.createElement( 'canvas' );
  c = canvas.getContext( '2d' );
  container = document.createElement( 'div' );
  container.className = "container";
  
  document.body.appendChild( container );
  container.appendChild(canvas);
  
  resetCanvas();
  
  c.strokeStyle = "#ffffff";
  c.lineWidth =2;
}

function mouseOver(minX, minY, maxX, maxY){
  return(mouseX>minX&&mouseY>minY&&mouseX<maxX&&mouseY<maxY);
}

function sendControls(){
  if(sendFlag == true){
    leftVector.x = Math.min(Math.max(parseInt(leftVector.x), -255), 255);
    leftVector.y = Math.min(Math.max(parseInt(leftVector.y), -255), 255);
    
    tankDrive(leftVector.x, -leftVector.y);
    if(leftMot > 0) leftMot += 70;
    if(leftMot < 0) leftMot -= 70;
    if(rightMot > 0) rightMot += 70;
    if(rightMot < 0) rightMot -= 70;
    leftMot = Math.min(Math.max(parseInt(leftMot), -255), 255);
    rightMot = Math.min(Math.max(parseInt(rightMot), -255), 255);
    
    // GET request for motor control
    var baseHost = 'http://192.168.4.1';
    const query = `${baseHost}/control?var=motor&val=${leftVector.x}&val2=${leftVector.y*-1}`;

    if (leftVector.x != 0 && leftVector.y != 0) {
      fetch(query)
        .then(response => {
          console.log(`request to ${query} finished, status: ${response.status}`)
        })
      sendFlag = false;
    }
  }
}

    </script>
	<div style="position:relative;z-index:0;">
      <canvas id="stream" width="800" height="600" style="z-index: -1;"> </canvas>
      <img src="http://192.168.4.1:81" width="800" style="z-index: 2;position:absolute;display:none" id="streamImg">
    </div>
	<script>resizeStream();</script>
  </body>
</html>
)=====";

  size_t index_blackeye_html_len = sizeof(index_blackeye_html)-1;