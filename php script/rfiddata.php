<?php
foreach ($_REQUEST as $key => $value)
{
    if ($key == "data") {
        $yourdata = $value;
    }
}
//function to convert hex values to ASCII
function hex2str($hex) {
    $str = '';
    for($i=0;$i<strlen($hex);$i+=2) $str .= chr(hexdec(substr($hex,$i,2)));
    return $str;
}
//replaces the blank space in the data recieved from the Arduino with ''
$string = str_replace(' ', '', $yourdata);

// takes a string finds where its seperated ',' and adds it to an array
// Converts string seperated by ',' into array of individual values that are seperated by commas
$exploded = explode( ',', $string);

// gets the size of the array
$max = sizeOf($exploded);

// loops through the array converting each string to ASCII
for($i = 0; $i < $max; $i++){
	$converted[] = hex2str($exploded[$i]);
}

// implodes converts array to a string and seperates the values with a comma
$stringtosend=implode(",",$converted);

// Removes the 0's from the string
$stringtosendwozeros = str_replace('0', '', $stringtosend);

//setting variables ready to send 
$to      = 'lukedjones23@gmail.com';
$subject = 'Pizza Ingredients';
$message = $stringtosendwozeros;
$headers = 'From: webmaster@example.com' . "\r\n" .
    'Reply-To: webmaster@example.com' . "\r\n" .
    'X-Mailer: PHP/' . phpversion();

//sends the variables above to a mail server hosted on XAMPP 
mail($to, $subject, $message, $headers);
?>