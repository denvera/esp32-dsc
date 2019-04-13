function updateSettingsForm () {
  if (document.getElementById("servertype").value.toUpperCase() == "CLOUD") {
    console.log("Show cloud elements");
  }

}

window.addEventListener('load', function() {
    document.getElementById("servertype").selectedIndex = document.getElementById("servertype-" + document.getElementById("servertype").getAttribute("value")).index;    
    document.getElementById("servertype").addEventListener('change', updateSettingsForm, false);
});
