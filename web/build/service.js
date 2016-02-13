$(document).ready(function() {
	
	(function worker() {
		$.getJSON('ajax/service', function(data) {
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
					case "heater_on":
						document.getElementById(key).innerHTML = val.toString() + " s.";
						break;
					case "temp":
						document.getElementById(key).innerHTML = val.toFixed(2) + "&deg;C";
						break;
					case "max_temp":
						document.getElementById(key).innerHTML = val.toString() + "&deg;C";
						break;
					case "vcc_val":
						document.getElementById(key).innerHTML = val.toFixed(2) + "V";
						break;
					case "vcc_min":
						document.getElementById(key).innerHTML = val.toFixed(2) + "V";
						break;
					case "vcc_corr":
						document.getElementById(key).innerHTML = val.toString() + "mV";
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
});