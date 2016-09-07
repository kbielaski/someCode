<?php
class handleButtons{

  private $matrix;
  private $colNames;

  public function __construct(){
    $this->matrix = array();
    $this->colNames = array("1" => "1", "2" => "2", "3" =>"3");
  }

  function makeHTMLTable($query){
 
    /*
    //requires that the $matrix and $colNames exist
    echo '<!DOCTYPE html>
<html>
<title>Secure Search Front End Tool</title>
<h1 style="font-size:300%;text-align:center;">Secure Search Results: ' . $query . '</h1>
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


	echo $this->matrix[0][0];
	echo "hello" . strval(sizeof($this->matrix));
  
      echo'</body>
</html>';*/
  }

  function submitQuery($preCalcDropdown, $displayValue, $dataset){
    echo '<!DOCTYPE html>
<html><body>';

    //these if statements are to select the correct dataset for the preCalculated Search
 
    //make sure index_search and parser are up to date
    $index_search = '/home/kellybielaski/data/SecureSearch/indexer/index_search';
    $parser = '/home/kellybielaski/data/SecureSearch/indexer/parser';
    $doclocation = '/silo/datasets/indices';

    $query = $displayValue;
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

      echo $query . " " . $dataset;

      $output = `echo $query | $index_search --index /silo/datasets/indices/$dataset/index | $parser $query $dataset 0`;
      //$output = shell_exec("echo " . $query . " | " . $index_search . " --index /silo/datasets/indices/" . $dataset . "/index | " . $parser . " " . $query . " " . $dataset ." 0");
 

      //the problem is that the output isn't being completed by the server
      echo "<pre>" . $output . "</pre>";
     //$dirOutput = `dir`;
     //echo "<pre>" . $dirOutput . "</pre>";



      //now I basically have the contents of the table in my $output and I want to make this into the matrix
      unset($this->colNames);
      $this->colNames = array("docid", "qid", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32", "33");   //get better descriptors for each of the columns 
    }
    //when it is preCalculated from aquaint
    elseif(in_array($preCalcDropdown, $aquaint_queries)){
      $dataset = "aquaint";
      $preCalc = "true";
      $query = str_replace(' ', '+', $_GET["displayValue"]);
      //echo shell_exec('echo ' . $query . ' | ' . $index_search . ' --index /silo/datasets/indices/' . $dataset . '/index | ' .  $parser . " " . $query . " " . $dataset . " " . "1");
    }
    //when it is preCalculated from trec08
    elseif(in_array($preCalcDropdown, $trec08_queries)){
      $dataset = "trec08";
      $preCalc = "true";
      $query = str_replace(' ', '+', $_GET["displayValue"]);
      //echo shell_exec('echo ' . $query . ' | ' . $index_search . ' --index /silo/datasets/indices/' . $dataset . '/index | ' .  $parser . " " . $query . " " . $dataset . " " . "1");
    }
    //when it is preCalculated from trec45
    elseif(in_array($preCalcDropdown, $trec45_queries)){
      $dataset = "trec45";
      $preCalc = "true";
      $query = str_replace(' ', '+', $_GET["displayValue"]);
      //echo shell_exec('echo ' . $query . ' | ' . $index_search . ' --index /silo/datasets/indices/' . $dataset . '/index | ' .  $parser . " " . $query . " " . $dataset . " " . "1");

    }
    else{
      echo 'ERROR: incorrect input syntax';
    }

    //now from here make the output into a matrix
    $line = explode("/n",$output);
    unset($this->matrix);
    $this->matrix = array();
    foreach($line as $item){
      $nthLine = array();
      $boxes = explode(" ", $item);
      foreach($boxes as $addbox){
	array_push($nthLine, $addbox);
      }
      if(sizeof($nthLine) > 3){
	$docid = $nthLine[sizeof($nthLine)-2];
      }
      array_unshift($nthLine, $docid);
      array_pop($nthLine);
      array_pop($nthLine);
      array_push($this->matrix, $nthLine);
    }


  
      echo'</body>
</html>';

      //now that I have the results formatted into a matrix, I can call the html table maker function to put my results in
      //the matrix must be saved as a private variable, and you must pass in the array of the header of the table
  }





  }





$handler = new handleButtons();

//I am using these values to test
$_GET["preCalcDropdown"] = "default";
$_GET["displayValue"] = "hello";
$_GET["dataset"] = "trec45";

//the preCalculated Queries don't work yet
if(isset($_GET["preCalcDropdown"]) and isset($_GET["displayValue"]) and isset($_GET["dataset"])){
  $handler->submitQuery($_GET["preCalcDropdown"], $_GET["displayValue"], $_GET["dataset"]);
  $handler->makeHTMLTable($_GET["displayValue"]);
}



?>
