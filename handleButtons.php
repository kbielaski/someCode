<?php
class handleButtons{

  private $query;
  private $matrix;
  private $colNames;

  public function __construct(){
    $this->matrix = array();
    $this->colNames = array("1" => "1", "2" => "2", "3" =>"3");
  }

  function makeHTMLTable(){
 
    /*
    //requires that the $matrix and $colNames exist
    echo '<!DOCTYPE html>
<html>
<title>Secure Search Front End Tool</title>
<h1 style="font-size:300%;text-align:center;">Secure Search Results: ' . $this->query . '</h1>
<head>
<style>
table {
    font-family: arial, sans-serif;
    border-collapse: collapse;
    width: 100%;
}

td, th {
    border: 1px solid #dddddd;
    text-align: left;
    padding: 8px;
}

tr:nth-child(even) {
    background-color: #dddddd;
}
</style>
</head>';
    //create the buttons
    echo "\n<body>\n<table>\n";
    echo "  <tr>\n";
    foreach($this->colNames as $name){
	echo '    <th>' . $name . '</th>' . "\n";
      }
    echo "  </tr>\n";
   foreach($this->matrix as $aline){
     echo "  <tr>\n";
	foreach($aline as $box){
	  echo '    <td>' . $box . '</td>' . "\n";
	}
	echo "  </tr>\n";
	}
	echo '</table>' . "\n";
      echo'</body>
</html>';*/
  }

  function reOrderMatrix($columnNum, $lth){

    //if lth is set sort low to high, if not, sort high to low
    if($lth == "1"){
      //I don't know how to sort by column
      //asort($this->matrix, function($a, $b){ return $a[$columnNum]-$b[$columnNum]});
    }
    else{
      //arsort($this->matrix,function($a, $b){ return $b[$columnNum]-$a[$columnNum]});
    }
    //overloading found on http://stackoverflow.com/questions/2699086/sort-multi-dimensional-array-by-value
  }

  function submitQuery($preCalcDropdown, $displayValue, $dataset){
    //makes the beginning of the table
    //requires that the $matrix and $colNames exist
    $this->query = $displayValue;
     echo '<!DOCTYPE html>
<html>
<title>Secure Search Front End Tool</title>
<h1 style="font-size:300%;text-align:center;">Secure Search Results: ' . $this->query . '</h1>
<head>
<style>
table {
    font-family: arial, sans-serif;
    border-collapse: collapse;
    width: 100%;
}

td, th {
    border: 1px solid #dddddd;
    text-align: left;
    padding: 8px;
}

tr:nth-child(even) {
    background-color: #dddddd;
}
</style>
</head>';
    //create the buttons
    echo "\n<body>\n<table>\n";
    echo "  <tr>\n";

    //these if statements are to select the correct dataset for the preCalculated Search
 
    //make sure index_search and parser are up to date
    $index_search = '/home/kellybielaski/data/SecureSearch/indexer/index_search';
    $parser = '/home/kellybielaski/data/SecureSearch/indexer/parser';
    $doclocation = '/silo/datasets/indices';

    $preCalc = "T/F";

    $trec08 = file_get_contents('/silo/datasets/ranking_data/queries.08enterprise.CE051-127.txt',true);
    $trec45 = file_get_contents('/silo/datasets/ranking_data/queries.04robust.301-450_601-700.txt',true);
    //$aquaint = file_get_contents('/silo/datasets/ranking_data/queries.aquaint.303-589_50.txt',true);
    $aquaint = "nothing in here";
    $trec08_queries = explode("\n",$trec08);
    $trec45_queries = explode("\n",$trec45);
    $aquaint_queries = explode("\n",$aquaint);




    //when it is not a preCalculated Query
    if($preCalcDropdown=="default"){
      $dataset = $dataset;
      $preCalc = "false";

      unset($this->colNames);
      $this->colNames = array("docid", "qid", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32", "33");   //get better descriptors for each of the columns 
      foreach($this->colNames as $name){
	echo '    <th>' . $name . '</th>' . "\n";
      }
    echo "  </tr>\n";
      unset($this->matrix);
      $this->matrix = array();

      $filename = shell_exec("echo " . $this->query . " | " . $index_search . " --index /silo/datasets/indices/" . $dataset . "/index | " . $parser . " " . $this->query . " " . $dataset ." 0");
      //$f_res = fopen("/tmp/filesa22tV", "r");
      $filename = str_replace("\n", '', $filename);
      $f_res = fopen($filename, "r");
      //while(!feof($f_res)){
	$line = fgets($f_res);
	echo $line;
	$arrOfLine = explode(" ",$line);
	echo "  <tr>\n";
	array_pop($arrOfLine);
	$docid = $arrOfLine[sizeof($arrOfLine)-1];
	array_unshift($arrOfLine, $docid);
	array_pop($arrOfLine);
	foreach($arrOfLine as $stat){
	  echo '   <td>' . $stat . '</td>' . "\n";
	}
	fclose($f_res);
	unlink($filename);
	echo "  </tr>\n";
	array_push($this->matrix, $arrOfLine);
	//}
	array_pop($this->matrix);
    }





    //when it is preCalculated from aquaint
    elseif(in_array($preCalcDropdown, $aquaint_queries)){
      $dataset = "aquaint";
      $preCalc = "true";
      $this->query = str_replace(' ', '+', $_GET["displayValue"]);
      //echo shell_exec('echo ' . $this->query . ' | ' . $index_search . ' --index /silo/datasets/indices/' . $dataset . '/index | ' .  $parser . " " . $this->query . " " . $dataset . " " . "1");
    }
    //when it is preCalculated from trec08
    elseif(in_array($preCalcDropdown, $trec08_queries)){
      $dataset = "trec08";
      $preCalc = "true";
      $this->query = str_replace(' ', '+', $_GET["displayValue"]);
      //echo shell_exec('echo ' . $this->query . ' | ' . $index_search . ' --index /silo/datasets/indices/' . $dataset . '/index | ' .  $parser . " " . $this->query . " " . $dataset . " " . "1");
    }
    //when it is preCalculated from trec45
    elseif(in_array($preCalcDropdown, $trec45_queries)){
      $dataset = "trec45";
      $preCalc = "true";
      $this->query = str_replace(' ', '+', $_GET["displayValue"]);
      //echo shell_exec('echo ' . $this->query . ' | ' . $index_search . ' --index /silo/datasets/indices/' . $dataset . '/index | ' .  $parser . " " . $this->query . " " . $dataset . " " . "1");

    }
    else{
      echo 'ERROR: incorrect input syntax';
    }

    
    echo '</table>' . "\n";
    echo'</body>
</html>';
    
  }


  }





$handler = new handleButtons();

//I am using these values to test
$_GET["preCalcDropdown"] = "default";
$_GET["displayValue"] = "hello";
$_GET["dataset"] = "trec45";

//the preCalculated Queries don't work yet
if(isset($_GET["preCalcDropdown"]) and isset($_GET["displayValue"]) and isset($_GET["dataset"])){
  /*echo $_GET["preCalcDropdown"];
  echo $_GET["displayValue"];
  echo $_GET["dataset"];*/
  $handler->submitQuery($_GET["preCalcDropdown"], $_GET["displayValue"], $_GET["dataset"]);

}

//if one of the columns buttons are set, then change the matrix and then print the new matrix
if(isset($_GET["col"]) and isset($_GET["lth"])){
  $handler->reOrderMatrix($_GET["col"], $_GET["lth"]);
  $handler->makeHTMLTable();
}

//also handle press to the button that has a link to 


?>
