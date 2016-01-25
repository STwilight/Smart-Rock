$(document).ready(function () {
	ToggleReadonly($('input[checked=checked]').val());
	$('input[name=ap_mode]').on('click', function() {
		$('input[name=ap_mode]').each(function(e) {
			$(e).removeAttr('checked');
		})
		$(this).attr('checked', 'checked');
		ToggleReadonly($(this).val());
	})
});

function ToggleReadonly(value) {
	if(value == '0') {
		$('.form-control').each(function() {
			if (this.name.substr(0, 3) == "ap_")
				$(this).attr('readonly', 'readonly');
		})
	}
	else
	{
		$('.form-control').each(function(){
			if (this.name.substr(0, 3) == "ap_")
				$(this).removeAttr('readonly');
		})
	}
}