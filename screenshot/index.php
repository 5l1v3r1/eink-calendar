<?php
require './../vendor/autoload.php';

use Knp\Snappy\Image;

$url = isset($_GET['u']) ? $_GET['u'] : null;

$brightness = isset($_GET['b']) && ($_GET['b']>50 && $_GET['b']<200) ? $_GET['b'] : 100;

$eink_model = isset($_GET['eink']) ? $_GET['eink'] : '';
$bitdepth = 1;

switch($eink_model) {
  case 'GxGDEW042T2':
   $displayWidth = 400;
   $displayHeight = 300;
   $zoomFactor = isset($_GET['z']) ? $_GET['z'] : '.4';
   $bitdepth = 4;
  break; 
  case 'GxGDEW027C44':
   $displayWidth = 264;
   $displayHeight = 176;
   $zoomFactor = isset($_GET['z']) ? $_GET['z'] : '.6';
   $bitdepth = 4;
  break;
  case 'GxGDEW075T7':
   $displayWidth = 800;
   $displayHeight = 480;
   $zoomFactor = isset($_GET['z']) ? $_GET['z'] : '1';
  break;
  default:
   // Default values in case eink model is not received
   $displayWidth = 640;
   $displayHeight = 384;
   $zoomFactor = isset($_GET['z']) ? $_GET['z'] : '.8';
  break;
}

$cacheEnabled = false;
$imageBasePath = './screenshots';
if (!$url) {
    exit('Url not defined');
}
// CACHE Validity
$validity = 2 * 60 * 60; // 2 * 3600s = 2 hours

$start = microtime(true);
$host = parse_url($url, PHP_URL_HOST);
$path = urlencode(parse_url($url, PHP_URL_PATH));
if ($path == '') $path = 'index';
if (!file_exists($imageBasePath.'/'.$host)) {
    mkdir($imageBasePath.'/'.$host, 0777, true);
}
header('Content-Type:image/bmp');

$filename = $imageBasePath.'/'.$host.'/'.$path.'.bmp';
if($cacheEnabled && file_exists($filename) && filectime($filename) > time() - $validity) {
    // cache is valid
    $file = file_get_contents($filename);
    exit($file);
}
//exit(print_r(Imagick::getVersion()));
$image = new Image();
$image->setBinary('/usr/local/bin/wkhtmltoimage');
$image->setOption('disable-smart-width', true);
$image->setOption('format', 'bmp');
$image->setOption('width', $displayWidth);
$image->setOption('height', $displayHeight);
$image->setOption('zoom', $zoomFactor);
// Set User agent (It does not work like this)
$image->setOption('custom-header', 'User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/60.0.3112.113 Safari/537.36');
// Last Safari
//$image->setOption('custom-header', 'User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9) AppleWebKit/537.71 (KHTML, like Gecko) Version/7.0 Safari/537.71');
$image->setOption('custom-header-propagation', true);
$image->setOption('disable-javascript', false);
// Initialize Imagick
$imageX = new Imagick();

try {
    $out = $image->getOutput($url);
} catch (RuntimeException $exception) {
    $draw = new ImagickDraw();
    $pixel = new ImagickPixel('white');
    $imageX->newImage($displayWidth, $displayHeight, $pixel);

    $imageX->annotateImage($draw, 10, 20, 0, $exception->getMessage());
    $imageX->setImageFormat('bmp');

    $imageX->setImageChannelDepth(Imagick::CHANNEL_GRAY, 4);
    $imageX->setImageType(Imagick::IMGTYPE_GRAYSCALE);
    exit($imageX);
}

$imageX->readImageBlob($out);
$saturation = 100;
$hue = 100;

$imageX->modulateImage($brightness, $saturation, $hue);
// This quantize returns a 1-bit image but does not work on 2.7 display
if ($bitdepth === 1) {
  $imageX->quantizeImage(2,     // Number of colors  
    Imagick::COLORSPACE_GRAY, // Colorspace
    8,                        // Depth tree  
    true,                     // Dither
    false);
}
// Commenting this lines drops a 24-bit BMP
  $imageX->posterizeimage(2, false);

try {
  $imageX->writeimage($filename);
} catch (Exception $exception) {
  // Do nothing: Sometimes comes a message with filename too large.
}
//echo( microtime(true) - $start );
echo $imageX;
