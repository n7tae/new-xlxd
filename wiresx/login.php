<?php
// Initialize the session
session_start();

// Check if the user is already logged in, if yes then redirect him to frequency page
if(isset($_SESSION["loggedin"]) && $_SESSION["loggedin"] === true){
  header("location: frequency.php");
  exit;
}

// Include config file
require_once "configure.php";

// Define variables and initialize with empty values
$callsign = $password = "";
$callsign_err = $password_err = "";

// Processing form data when form is submitted
if ($_SERVER["REQUEST_METHOD"] == "POST") {

    // Check if callsign is empty
    if (empty(trim($_POST["callsign"]))) {
        $callsign_err = "Please enter your callsign.";
    } else {
        $callsign = strtoupper(trim($_POST["callsign"]));
    }

    // Check if password is empty
    if (empty(trim($_POST["password"]))) {
        $password_err = "Please enter your password.";
    } else {
        $password = trim($_POST["password"]);
    }

    // Validate credentials
    if (empty($callsign_err) && empty($password_err)) {
        // Prepare a select statement
        $sql = "SELECT callsign, password FROM ysfnodes WHERE callsign = ?";

        if ($stmt = mysqli_prepare($link, $sql)) {
            // Bind variables to the prepared statement as parameters
            mysqli_stmt_bind_param($stmt, "s", $param_callsign);

            // Set parameters
            $param_callsign = $callsign;

            // Attempt to execute the prepared statement
            if (mysqli_stmt_execute($stmt)) {
                // Store result
                mysqli_stmt_store_result($stmt);

                // Check if callsign exists, if yes then verify password
                if (mysqli_stmt_num_rows($stmt) == 1) {
                    // Bind result variables
                    mysqli_stmt_bind_result($stmt, $callsign, $hashed_password);
                    if (mysqli_stmt_fetch($stmt)) {
                        if (password_verify($password, $hashed_password)) {
                            // Password is correct, so start a new session
                            session_start();

                            // Store data in session variables
                            $_SESSION["loggedin"] = true;
                            $_SESSION["callsign"] = $callsign;

                            // Redirect user to frequency page
                            header("location: frequency.php");
                        } else {
                            // Display an error message if password is not valid
                            $password_err = "The password you entered was not valid.";
                        }
                    }
                } else {
                    // Display an error message if callsign doesn't exist
                    $callsign_err = "No account found with that callsign.";
                }
            } else {
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
    <title>Login</title>
    <link rel="stylesheet" href="bootstrap.css">
    <style type="text/css">
        body{ font: 14px sans-serif; }
        .wrapper{ width: 350px; padding: 20px; }
    </style>
</head>
<body>
    <div class="wrapper">
        <h2>Login</h2>
        <p>Login with your hot-spot callsign.</p>
        <form action="<?php echo htmlspecialchars($_SERVER["PHP_SELF"]); ?>" method="post">
            <div class="form-group <?php echo (!empty($callsign_err)) ? 'has-error' : ''; ?>">
                <label>Username</label>
                <input type="text" name="callsign" class="form-control" value="<?php echo $callsign; ?>">
                <span class="help-block"><?php echo $callsign_err; ?></span>
            </div>
            <div class="form-group <?php echo (!empty($password_err)) ? 'has-error' : ''; ?>">
                <label>Password</label>
                <input type="password" name="password" class="form-control">
                <span class="help-block"><?php echo $password_err; ?></span>
            </div>
            <div class="form-group">
                <input type="submit" class="btn btn-primary" value="Login">
            </div>
            <p>Don't have an account? <a href="register.php">Sign up now</a>.</p>
        </form>
    </div>
</body>
</html>
