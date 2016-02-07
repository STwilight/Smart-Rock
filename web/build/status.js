$(document).ready(function() {
	
	(function worker() {
		$.getJSON('ajax/status', function(data) {
			var ap_mode = false;
			var messages = "";
			
			$.each(data, function(key, val) {
				var target = $("#" + key);
				switch (key) {
					case "sens_err":
						if (val)
							messages += '<div class="alert alert-danger" role="alert">Temperature sensor error!</div>';
						break;					
					case "heater":
						if (val) {
							target.removeClass("label-danger").addClass("label-success");
							document.getElementById(key).innerHTML = "On";
						}
						else {
							target.removeClass("label-success").addClass("label-danger");
							document.getElementById(key).innerHTML = "Off";
						}
						break;
					case "temp":
						document.getElementById(key).innerHTML = val.toString() + "&deg;C";
						break;
					case "max_temp":
						document.getElementById(key).innerHTML = val.toString() + "&deg;C";
						break;
					case "ap_mode":
						ap_mode = val;
						if (val) {
							target.removeClass("label-danger").addClass("label-success");
							document.getElementById(key).innerHTML = "On";
						}
						else {
							target.removeClass("label-success").addClass("label-danger");
							document.getElementById(key).innerHTML = "Off";
						}
						break;
					case "ap_ssid":
						if (ap_mode)
							document.getElementById("ap_status").innerHTML = '<p>AP SSID: <span id="' + key + '" class="label label-default">' + val + '</span><p>';
						else
							document.getElementById("ap_status").innerHTML = "";
						break;
					case "ap_ip":
						if (ap_mode)
							document.getElementById("ap_status").innerHTML += '<p>AP IP: <span id="' + key + '" class="label label-default">' + val + '</span><p>';
						else
							document.getElementById("ap_status").innerHTML = "";
						break;
					case "st_err":
						if (val)
							messages += '<div class="alert alert-danger" role="alert">Failed to connect to specified Wi-Fi network!</div>';
						break;							
					default:
						document.getElementById(key).innerHTML = val;
						break;
				}
			});
			document.getElementById("warnings").innerHTML = messages;
			setTimeout(worker, 500);
		});
	})();
	
	$(".switch-freq").click(function() {
		var freq = $(this).attr('data-value');
		$.getJSON('ajax/frequency', {value: freq}, function(data) {
			$("#display-freq").text(data.value);
		});
	});
});