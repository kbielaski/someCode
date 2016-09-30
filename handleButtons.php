<?php
class handleButtons{

  private $query;
  private $dataset;
  private $matrix;
  private $colNames;

  public function __construct(){
    //default matrix of just an empty three columned table
    $this->matrix = array();
    $this->colNames = array("1" => "1", "2" => "2", "3" =>"3");
  }

  public function getMatrix(){
    return $this->matrix;
  }
  function createMatrix(){
    $this->colNames = array("first", "20", "three", "004", "last"); 
    $this->matrix = array(
			  array(1,2,3,4,5),
			  array(6,7,8,9,10),
			  array(11,12,13,14,15),
			  array(16,17,18,19,20),
			  array(21,22,23,24,25)
			  );
    $this->query = "test";
    $this->dataset = "test";

  }

  function makeHTMLTable(){
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
td {
    border: 1px solid #dddddd;
    text-align: left;
    padding: 8px;
}
th {
    border: 1px solid #dddddd;
    text-align: center;
    padding: 0px;
}
tr:nth-child(even) {
    background-color: #dddddd;
}
}
</style>
</head>' . "\n";
    //dropdown menu of the choices for what happens when you click on a column
    echo ' <form action= "searchResults.php" method = "get">' . "\n";
    echo '  <select name="selectColSort" method="get"> 
   <option value="asc">ascending</option>
   <option value="dec">descending</option>
   <option value="delete">delete column</option>
  </select>' . "\n";
    //button to do a new search
    //echo ' <form action= "searchResults.php" method = "get">' . "\n";

    echo '<br>' . "\n";
    echo '<br>';
    echo "\n<body>\n<table>\n";
    echo "  <tr>\n";
    foreach($this->colNames as $name){
      echo '    <th>' . "\n";
      echo '     <input type="submit" name="sortBy" value="' . $name . '">' . "\n";
      echo '     </form>' . "\n";
      echo '    </th>' . "\n";
    }
    echo "  </tr>\n";
    foreach($this->matrix as $row){
      echo "  <tr>\n";
      foreach($row as $entry){
	echo '   <td>' . $entry . "</td>\n";
      }
      echo "  </tr>\n";
    }
    echo '</table>' . "\n";
    echo'</body>
</html>';
  }
  function deleteCol($name){
    $index = array_search($name, $this->colNames);
    /*for($x=0; $x<count($this->colNames)-1; $x++){
      if($this->colNames[$x] == $name){
      $index = $x;
      }
      }*/
    unset($this->colNames[$index]);
    $newMatrix = array();
    foreach($this->matrix as $row){
      unset($row[$index]);
      array_push($newMatrix, $row);
    }
    $this->matrix = $newMatrix;
  }

  function reOrderMatrix($colNum, $order){

    //if lth is set sort low to high, if not, sort high to low
    if($order == "1"){
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

    //add at the top a box where it says what happens when you click on the button, either, you can get rid of the column because it is all zeros, or you can sort from dec or ascending order
    //make sure the other types of queries work
    //aquaint has to work
    //when you click on the docid you get doc
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
td {
    border: 1px solid #dddddd;
    text-align: left;
    padding: 8px;
}
th {
    border: 1px solid #dddddd;
    text-align: center;
    padding: 0px;
}
tr:nth-child(even) {
    background-color: #dddddd;
}
}
</style>
</head>' . "\n";
    echo ' <form action= "searchResults.php" method = "get">' . "\n";
    echo '  <select name="selectColSort" method="get">
   <option value="asc">ascending</option>
   <option value="dec">descending</option>
   <option value="delete">delete column</option>
  </select>' . "\n";
    echo '<br>'. "\n";
    echo '<br>' . "\n";
    echo "\n<body>\n<table>\n";
    echo "  <tr>\n";

    //these if statements are to select the correct dataset for the preCalculated Search
 
    //make sure index_search and parser are up to date
    $index_search = '/var/www/cgi-bin/index_search';
    $parser = '/var/www/cgi-bin/parser';
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
      $this->dataset = $dataset;

      unset($this->colNames);
      $this->colNames = array("docid", "qid", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32", "33");   //get better descriptors for each of the columns 
      foreach($this->colNames as $name){
     	echo '    <th>' . "\n";
	echo '     <input type="submit" name="sortBy" value="' . $name . '">' . "\n";
	echo '     </form>' . "\n";
	echo '    </th>' . "\n";

	//echo '    <th>' . $name . '</th>' . "\n";
      }
      echo "  </tr>\n";
      unset($this->matrix);
      $this->matrix = array();

      $filename = shell_exec("echo " . $this->query . " | " . $index_search . " --index /silo/datasets/indices/" . $this->dataset . "/index | " . $parser . " " . $this->query . " " . $this->dataset ." 0");
      if($filename == NULL){
	echo "problem with shell_exec";
      }
      
      $filename = str_replace("\n", '', $filename);
      $f_res = fopen($filename, "r");
      while(!feof($f_res)){
	$line = fgets($f_res);
	if ($line != ""){
	  $arrOfLine = explode(" ",$line);
	  echo "  <tr>\n";
	  array_pop($arrOfLine);
	  $docid = $arrOfLine[sizeof($arrOfLine)-1];
	  array_unshift($arrOfLine, $docid);
	  array_pop($arrOfLine);
	  foreach($arrOfLine as $stat){
	    echo '   <td>' . $stat . '</td>' . "\n";
	  }
	  echo "  </tr>\n";
	  array_push($this->matrix, $arrOfLine);
	}
      }
      array_pop($this->matrix);
      fclose($f_res);
      unlink($filename);
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




//I am using these values to test

/*$_GET["selectColSort"] = "delete";
  $_GET["sortBy"] = "three";*/

/*$_GET["preCalcDropdown"] = "default";
$_GET["displayValue"] = "hello";
$_GET["dataset"] = "trec08";*/


//the preCalculated Queries don't work yet


if(isset($_GET["preCalcDropdown"]) and isset($_GET["displayValue"]) and isset($_GET["dataset"])){
  $matrix = new handleButtons();
  $matrix->submitQuery($_GET["preCalcDropdown"], $_GET["displayValue"], $_GET["dataset"]);
}

if(isset($_GET["selectColSort"]) and isset($_GET["sortBy"])){
    //$matrix->createMatrix();
    if($_GET["selectColSort"] == "delete"){
      $matrix->deleteCol($_GET["sortBy"]);
    }
    if($_GET["selectColSort"] == "asc"){
      $matrix->reOrderMatrix($_GET["sortBy"]);
    }
    if($_GET["selectColSort"] == "delete"){
      $matrix->reOrderMatrix($_GET["sortBy"]);
    }
    $matrix->makeHTMLTable();
  }


else{
  echo "ERROR: all search fields are not filled out";
}




?>
