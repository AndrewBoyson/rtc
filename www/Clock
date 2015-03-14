<h1><span id="local"></span></h1>
<h1><span id="utc"></span></h1>
<br/>
<h2>Offset <span id="offset"></span> ms</h2>

<script>
	//Static variables
	var topLeapSecond1970 = ~UtcTopLeapSecond1970~;
	var applyLeap;
	var lastSecond;
	
	var isDisplayTopLeap;
	var rtcMinusJavaMs;
	var rtcMinusJavaMsTopLeap;
	
	function displayTime() {
	
		//Fetch time and correct for the initial offset
		var today = new Date();
		var ms = isDisplayTopLeap ? rtcMinusJavaMsTopLeap : rtcMinusJavaMs
		ms += today.getUTCMilliseconds();
		today.setUTCMilliseconds(ms); //The datetime object will handle any resulting overflow
		
		//Calculate if this is the leap second
		var utcJavaMs1970 = today.valueOf(); //MilliSeconds since 1970
		var utcJavaSecond1970   = Math.floor(utcJavaMs1970 / 1000);
		isLeap = utcJavaSecond1970 == topLeapSecond1970;
		if (isLeap) applyLeap = true;
		
		//Subtract a second following the leap
		if (applyLeap) today.setUTCSeconds(today.getUTCSeconds() - 1);
		
		//Extract values
		var y=today.getUTCFullYear();
		var n=today.getUTCMonth() + 1;
		var d=today.getUTCDate();

		var h=today.getUTCHours();
		var m=today.getUTCMinutes();
		var s=today.getUTCSeconds();

		//Make the second 60 if need be	
		if (isLeap) s = 60;
		
		if (lastSecond != s)
		{
			//Format time
			n=formatNumbers00(n);
			d=formatNumbers00(d);
			h=formatNumbers00(h);
			m=formatNumbers00(m);
			s=formatNumbers00(s);
		
		
			//Display time
			document.getElementById('local').innerHTML = today.toString();
			document.getElementById('utc').innerHTML = y + "-" + n + "-" + d + " " + h + ":" + m + ":" + s + " UTC";
			document.getElementById('offset').innerHTML = rtcMinusJavaMs;
			lastSecond = s;
		}

		//Call again
		t=setTimeout(displayTime, 100);
	}
	
	function formatNumbers00(i) {
		if (i<10) i="0" + i;
		return i;
	}

	function displayTopLeap() {
		var today = new Date();
		rtcMinusJavaMsTopLeap = (topLeapSecond1970 - 5) * 1000 - today.valueOf();
		applyLeap = false;
		lastSecond = -1;
		isDisplayTopLeap = true;
	}
	
	function displayNow() {
		applyLeap = false;
		lastSecond = -1;
		isDisplayTopLeap = false;
	}
	
	var today = new Date();
	rtcMinusJavaMs = ~UtcMs1970~ - today.valueOf();
	displayNow();
	displayTime();

	var ajax;
	
	function requestAjax() {
		ajax=new XMLHttpRequest();
		ajax.onreadystatechange=handleAjaxResponse;
		ajax.open("GET", "/ajax?~~UtcMs1970~~", true);
		ajax.send();
	}
	function setAjaxTimer() {
		t=setTimeout(requestAjax, 60000); //Call again
	}
	function handleAjaxResponse() {
		if (ajax.readyState==4 && ajax.status==200)
		{
			utcMs1970 = parseInt(ajax.responseText);
			var today = new Date();
			rtcMinusJavaMs = utcMs1970 - today.valueOf();
			setAjaxTimer(); //Call again
		}
	}
	setAjaxTimer();
	
</script>
<br/>
<button type="button" onclick="displayTopLeap()">Display Top Leap</button>
<button type="button" onclick="displayNow()">Display Now</button>
~edit^ntpclients^NTP Clients~
~edit^fanvalues^Fan Values~
