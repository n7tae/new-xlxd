<?php

/*
 *  This dashboard is being developed by the DVBrazil Team as a courtesy to
 *  the XLX Multiprotocol Gateway Reflector Server project.
 *  The dashboard is based of the Bootstrap dashboard template.
*/

if (file_exists("./pgs/functions.php")) {
    require_once("./pgs/functions.php");
} else {
    die("functions.php does not exist.");
}
if (file_exists("./pgs/config.inc.php")) {
    require_once("./pgs/config.inc.php");
} else {
    die("config.inc.php does not exist.");
}

if (!class_exists('ParseXML')) require_once("./pgs/class.parsexml.php");
if (!class_exists('Node')) require_once("./pgs/class.node.php");
if (!class_exists('xReflector')) require_once("./pgs/class.reflector.php");
if (!class_exists('Station')) require_once("./pgs/class.station.php");
if (!class_exists('Peer')) require_once("./pgs/class.peer.php");
if (!class_exists('Interlink')) require_once("./pgs/class.interlink.php");

$Reflector = new xReflector();
$Reflector->SetFlagFile("./pgs/country.csv");
$Reflector->SetPIDFile($Service['PIDFile']);
$Reflector->SetXMLFile($Service['XMLFile']);

$Reflector->LoadXML();

if ($CallingHome['Active']) {

    $CallHomeNow = false;
    if (!file_exists($CallingHome['HashFile'])) {
        $Hash = CreateCode(16);
        $LastSync = 0;
        $Ressource = @fopen($CallingHome['HashFile'], "w");
        if ($Ressource) {
            @fwrite($Ressource, "<?php\n");
            @fwrite($Ressource, "\n" . '$LastSync = 0;');
            @fwrite($Ressource, "\n" . '$Hash     = "' . $Hash . '";');
            @fwrite($Ressource, "\n\n" . '?>');
            @fclose($Ressource);
            @exec("chmod 777 " . $CallingHome['HashFile']);
            $CallHomeNow = true;
        }
    } else {
        include($CallingHome['HashFile']);
        if ($LastSync < (time() - $CallingHome['PushDelay'])) {
            $Ressource = @fopen($CallingHome['HashFile'], "w");
            if ($Ressource) {
                @fwrite($Ressource, "<?php\n");
                @fwrite($Ressource, "\n" . '$LastSync = ' . time() . ';');
                @fwrite($Ressource, "\n" . '$Hash     = "' . $Hash . '";');
                @fwrite($Ressource, "\n\n" . '?>');
                @fclose($Ressource);
            }
            $CallHomeNow = true;
        }
    }

    if ($CallHomeNow || isset($_GET['callhome'])) {
        $Reflector->SetCallingHome($CallingHome, $Hash);
        $Reflector->ReadInterlinkFile();
        $Reflector->PrepareInterlinkXML();
        $Reflector->PrepareReflectorXML();
        $Reflector->CallHome();
    }
} else {
    $Hash = "";
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="description" content="<?php echo $PageOptions['MetaDescription']; ?>"/>
    <meta name="keywords" content="<?php echo $PageOptions['MetaKeywords']; ?>"/>
    <meta name="author" content="<?php echo $PageOptions['MetaAuthor']; ?>"/>
    <meta name="revisit" content="<?php echo $PageOptions['MetaRevisit']; ?>"/>
    <meta name="robots" content="<?php echo $PageOptions['MetaAuthor']; ?>"/>

    <meta http-equiv="content-type" content="text/html; charset=utf-8"/>
    <title><?php echo $Reflector->GetReflectorName(); ?> Reflector Dashboard</title>
    <link rel="icon" href="./favicon.ico" type="image/vnd.microsoft.icon">
    <!-- Bootstrap core CSS -->
    <link href="css/bootstrap.min.css" rel="stylesheet">

    <!-- Custom styles for this template -->
    <link href="css/dashboard.css" rel="stylesheet">

    <!-- HTML5 shim and Respond.js for IE8 support of HTML5 elements and media queries -->
    <!--[if lt IE 9]>
    <script src="https://oss.maxcdn.com/html5shiv/3.7.2/html5shiv.min.js"></script>
    <script src="https://oss.maxcdn.com/respond/1.4.2/respond.min.js"></script>
    <![endif]-->
    <?php

    if ($PageOptions['PageRefreshActive']) {
        echo '
   <script src="./js/jquery-3.7.1.min.js"></script>
   <script>
      var PageRefresh;

      function ReloadPage() {
         $.get("./index.php'.(isset($_GET['show'])?'?show='.$_GET['show']:'').'", function(data) {
            var BodyStart = data.indexOf("<bo"+"dy");
            var BodyEnd = data.indexOf("</bo"+"dy>");
            if ((BodyStart >= 0) && (BodyEnd > BodyStart)) {
               BodyStart = data.indexOf(">", BodyStart)+1;
               $("body").html(data.substring(BodyStart, BodyEnd));
            }
         })
            .always(function() {
               PageRefresh = setTimeout(ReloadPage, '.$PageOptions['PageRefreshDelay'].');
            });
      }';

	if (!isset($_GET['show']) || (($_GET['show'] != 'liveircddb') && ($_GET['show'] != 'reflectors') && ($_GET['show'] != 'interlinks') && ($_GET['show'] != 'livequadnet'))) {
            echo '
      PageRefresh = setTimeout(ReloadPage, ' . $PageOptions['PageRefreshDelay'] . ');';
        }
        echo '

      function SuspendPageRefresh() {
        clearTimeout(PageRefresh);
      }
   </script>';
    }
    if (!isset($_GET['show'])) $_GET['show'] = "";
    ?>
</head>
<body>
<?php if (file_exists("./tracking.php")) {
    include_once("tracking.php");
} ?>
<nav class="navbar navbar-inverse navbar-fixed-top">
    <div class="container-fluid">
        <div class="navbar-header">
            <button type="button" class="navbar-toggle collapsed" data-toggle="collapse" data-target="#navbar"
                    aria-expanded="false" aria-controls="navbar">
                <span class="sr-only">Toggle navigation</span>
                <span class="icon-bar"></span>
                <span class="icon-bar"></span>
                <span class="icon-bar"></span>
            </button>
            <span class="navbar-brand"><?php echo $Reflector->GetReflectorName(); ?> Multiprotocol Reflector (DExtra, DPlus and DCS)</span>
        </div>
        <div id="navbar" class="navbar-collapse collapse">
            <ul class="nav navbar-nav navbar-right">
                <li class="navbar-info">xrfd vQ<?php echo $Reflector->GetVersion(); ?> - Dashboard
                    v<?php echo $PageOptions['DashboardVersion']; ?></li>
                <li class="navbar-info">Service
                    uptime: <?php echo FormatSeconds($Reflector->GetServiceUptime()); ?></li>
            </ul>
        </div>
    </div>
</nav>

<div class="container-fluid">
    <div class="row">
        <div class="col-sm-3 col-md-2 sidebar">
            <ul class="nav nav-sidebar">
                <li<?php echo (($_GET['show'] == "users") || ($_GET['show'] == "")) ? ' class="active"' : ''; ?>><a
                            href="./index.php">Last Heard</a></li>
                <li<?php echo ($_GET['show'] == "repeaters") ? ' class="active"' : ''; ?>><a
                            href="./index.php?show=repeaters">Links (<?php echo $Reflector->NodeCount();  ?>)</a></li>
                <li<?php echo ($_GET['show'] == "livequadnet") ? ' class="active"' : ''; ?>><a
                            href="./index.php?show=livequadnet">QuadNet Live</a></li>
            </ul>
        </div>
        <div class="col-sm-9 col-sm-offset-3 col-md-10 col-md-offset-2 main">

            <?php
            if ($CallingHome['Active']) {
                if (!is_readable($CallingHome['HashFile']) && (!is_writeable($CallingHome['HashFile']))) {
                    echo '
         <div class="error">
            your private hash in ' . $CallingHome['HashFile'] . ' could not be created, please check your config file and the permissions for the defined folder.
         </div>';
                }
            }

            switch ($_GET['show']) {
                case 'users'      :
                    require_once("./pgs/users.php");
                    break;
                case 'repeaters'  :
                    require_once("./pgs/repeaters.php");
                    break;
                case 'livequadnet' :
                    require_once("./pgs/livequadnet.php");
                    break;
                 case 'peers'      :
                    require_once("./pgs/peers.php");
                    break;
                case 'reflectors' :
                    require_once("./pgs/reflectors.php");
                    break;
                default           :
                    require_once("./pgs/users.php");
            }

            ?>

        </div>
    </div>
</div>

<footer class="footer">
    <div class="container">
        <p><a href="mailto:<?php echo $PageOptions['ContactEmail']; ?>"><?php echo $PageOptions['ContactEmail']; ?></a>
        </p>
    </div>
</footer>
</body>
</html>
