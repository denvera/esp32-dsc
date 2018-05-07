<!doctype html>
<html lang="en">
<head>
<title>DSC Config</title>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, shrink-to-fit=no">
<link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.1/css/bootstrap.min.css">

</head>
<body>
<div id="main" class="container">
<form name="dscconf" action="/settings/settings.cgi" method="post">
<h4>Set node-dsc server or MQTT address</h4>
<br>
<div class="form-group">
  <label for="dscserver">Server Address</label>
  <input type="text" class="form-control" id="dscserver" name="dscserver" value="%dscserver%" placeholder="Server Address"> <br />
</div>
<div class="form-group">
  <label for="port">Server Address</label>
  <input type="text" class="form-control" id="port" name="port" value="%port%" placeholder="Port Number"> <br />
</div>
<div class="form-group">
  <label for="servertype">Server Address</label>
  <select class="form-control" id="servertype" name="servertype" value="%servertype%" placeholder="Server Address">
    <option>TCP</option>
    <option>MQTT</option>
  </select>
</div>

<input type="submit" name="submit" value="Set">
</form>
<form name="bootapp" action="/settings/reboot.cgi" method="post">
<div class="form-group">
<input type="submit" name="rebootfactory" value="Reboot to Factory">
</div>
</form>

</div>
</body>
</html>
