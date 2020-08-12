<?php
// Initialize the session
session_start();

// Check if the user is logged in, otherwise redirect to login page
if (!isset($_SESSION["loggedin"]) || $_SESSION["loggedin"] !== true) {
    header("location: login.php");
    exit;
}

// Include config file
require_once "configure.php";

// find current frequencies and initialize
$sql = "SELECT txfreq, rxfreq FROM ysfnodes WHERE callsign = ?";
if ($stmt = mysqli_prepare($link, $sql)) {
	// Bind variables to the prepared statement as parameters
	mysqli_stmt_bind_param($stmt, "s", $_SESSION["callsign"]);
	// Attempt to execute the prepared statement
	if (mysqli_stmt_execute($stmt)) {
		// Store result
		mysqli_stmt_store_result($stmt);

		// Check if callsign exists, if yes then verify password
		if (mysqli_stmt_num_rows($stmt) == 1) {
			// Bind result variables
			mysqli_stmt_bind_result($stmt, $tx_freq_hz, $rx_freq_hz);
			if (mysqli_stmt_fetch($stmt)) {
				$txfreq = $tx_freq_hz / 1000000.0;
				$rxfreq = $rx_freq_hz / 1000000.0;
			} else {
				die("Can't bind frequencies\n");
			}
		} else {
			die("Couldn't find one row for callsign\n");
		}
	} else {
		die("Trouble SELECTing row\n");
	}
	mysqli_stmt_close($stmt);
} else {
	die("Couldn't prepare SELECT statement\n");
}
?>

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Set Frequency</title>
    <link rel="stylesheet" href="bootstrap.css">
    <style type="text/css">
        body{ font: 14px sans-serif; text-align: center; }
    </style>
</head>
<body>
    <div class="page-header">
        <h1>Summary for <?php echo htmlspecialchars($_SESSION["callsign"]); ?></h1>
    </div>
    <div class="wrapper">
        <h2>Hot-Spot Frequencies Summary</h2>
        <p>Hot-spot Callsign: <?php echo htmlspecialchars($_SESSION["callsign"]); ?></p>
		<p>Transmit Frequency: <?php echo $txfreq; ?> MHz</p>
		<p>Receive Frequency: <?php echo $rxfreq; ?> MHz</p>
		<p>It can take up to 15 minutes for the frequency data to be available to the reflector. Please be patient.</p>
    </div>
    <p>
		<a href="frequency.php" class="btn btn-primary">Reset Frequencies</a>
        <a href="reset-password.php" class="btn btn-warning">Reset Your Password</a>
        <a href="logout.php" class="btn btn-danger">Sign Out of Your Account</a>
    </p>
</body>
</html>
