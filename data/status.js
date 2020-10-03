var tt =0;
var th = 0;
var init = false;
var TimeMap = new Map();
var TimeNameToId = new Map();
var DateMap = new Map();
var DateNameToId=new Map();
var lastSelectStart;
var lastSelectEnd;
var newid= 0;



function momentFormatted(date)
{
  return date.format('DD/MM/YYYY');
}

function updateStatus() {
    $.ajax({
      method: "GET",
      url: window.location.origin+"/get/status",
      success: function(data) {
        $('.temp-value').empty().append(`${data.tval}C | ${data.tartval}C`);
        $('.humid-value').empty().append(`${data.hval}%`);
        $('.pwm-value').empty().append(`${data.pwmval}%`);
        $('.fan-value').empty().append(data.fan?"ON":"OFF");
        $('.time-value').empty().append(data.time);
        $('.status-box').append(data.status);
        $('.status-box').scrollTop(99999);
      },
      complete: function(obj, status){
        setTimeout(updateStatus, 1000); 
      }
    });
    // you could choose not to continue on failure...
}

function turnHeatOn()
{
  var tobj = new Object();
  tobj["heat"] = true;
  $.ajax({
    method: "PUT",
    url: window.location.origin+"/set/heat",
    data: tobj
  });
}

function turnHeatOff()
{
  var tobj = new Object();
  tobj["heat"] = false;
  $.ajax({
    method: "PUT",
    url: window.location.origin+"/set/heat",
    data: tobj
  });
}

function updateHeat()
{
  var tobj = new Object();
  tobj["temp"] = $('.temptarget').val();
  tobj["pwmfr"] = $('.pwmfreq').val()
  $.ajax({
    method: "PUT",
    url: window.location.origin+"/set/temp",
    data: tobj
  });
}

function toggleFan()
{
  var tobj = new Object();
  $.ajax({
    method: "PUT",
    url: window.location.origin+"/set/fan",
    data: tobj
  });
}

function handleEnter(e)
{
  var keycode = (e.keyCode ? e.keyCode : e.which);
    if (keycode == '13') {
        var tobj = new Object();
        tobj.input = $('.serial-input').val();
        $.ajax({
          method: "PUT",
          url: window.location.origin+"/update/serialinput",
          data: tobj,
          success: function(data){
            $('.serial-input').val("");
          }
        });
    }
}

$(document).ready(function() {
    updateStatus();
});