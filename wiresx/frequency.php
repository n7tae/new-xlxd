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
$sql = "SELECT txfreq, rxfreq FROM users WHERE callsign = ?";
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

// Processing form data when form is submitted
if ($_SERVER["REQUEST_METHOD"] == "POST") {

    // Validate new password
    if ($txfreq > 1000.0 || $txfreq < 10.0) {
        $txfreq_err = "TX out of range.";
    }
    // Validate confirm password
    if ($rxfreq > 1000.0 || $rxfreq < 10.0) {
		$rxfreq_err = "RX out of range.";
    }

    // Check input errors before updating the database
    if (empty($txfreq_err) && empty($rxfreq_err)) {
        // Prepare an update statement
        $sql = "UPDATE users SET txfreq = ?, rxfreq = ? WHERE callsign = ?";

        if($stmt = mysqli_prepare($link, $sql)){
			// Bind variables to the prepared statement as parameters
			$tx_freq_hz = $_POST["txfreq"] * 1000000;
			$rx_freq_hz = $_POST["rxfreq"] * 1000000;
            mysqli_stmt_bind_param($stmt, "iis", $tx_freq_hz, $rx_freq_hz, $_SESSION["callsign"]);

            // Attempt to execute the prepared statement
            if(mysqli_stmt_execute($stmt)){
                // Frequencies updated successfully, go to summary
                header("location: finish.php");
                exit();
            } else{
                echo "Oops! Something went wrong. Please try again later.";
            }

            // Close statement
            mysqli_stmt_close($stmt);
        }
    }

    // Close connection
    mysqli_close($link);
}
?>

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Set Frequency</title>
    <link rel="stylesheet" href="bootstrap.css">
    <style type="text/css">
        body{ width: 350px; padding: 20px; }
    </style>
</head>
<body>
    <div class="page-header">
        <h1>Set Frequencies for <?php echo htmlspecialchars($_SESSION["callsign"]); ?></h1>
    </div>
    <div class="wrapper">
        <h2>Hot-Spot Frequencies</h2>
        <p>Set your hot-spot WiresX frequencies (in MHz) here.</p>
        <form action="<?php echo htmlspecialchars($_SERVER["PHP_SELF"]); ?>" method="post">
            <div class="form-group <?php echo (!empty($new_txfreq_err)) ? 'has-error' : ''; ?>">
                <label>Tx Frequency (MHz)</label>
                <input type="number" size="12" name="txfreq" min="10" max="1000" step="0.0005" class="form-control" value="<?php echo $txfreq; ?>">
                <span class="help-block"><?php echo $txfreq_err; ?></span>
            </div>
            <div class="form-group <?php echo (!empty($rxfreq_err)) ? 'has-error' : ''; ?>">
                <label>Rx Frequency (MHz)</label>
                <input type="number" size="12" name="rxfreq" min="10" max="1000" step="0.0005" class="form-control" value="<?php echo $rxfreq; ?>">
                <span class="help-block"><?php echo $rxfreq_err; ?></span>
            </div>
            <div class="form-group">
                <input type="submit" class="btn btn-primary" value="Submit">
                <a class="btn btn-link" href="finish.php">Cancel</a>
            </div>
        </form>
    </div>
    <p>
        <a href="reset-password.php" class="btn btn-warning">Reset Your Password</a>
        <a href="logout.php" class="btn btn-danger">Sign Out of Your Account</a>
    </p>
</body>
</html>
