<!DOCTYPE html>
<html>
<title>Secure Search Front End Tool</title>
<body>

	<h1 style="font-size:300%;text-align:center;">Secure Search</h1>

	<form action= "/cgi-bin/index_search.sh" method = "get">
		<b>Search Queries:</b><br>
		<input type="text" name="query"></input><br>
		<select name="dataset" method="get">
		  <option value="aquaint">aquaint</option>
		  <option value="trec08">trec08</option>
		  <option value="trec45">trec45</option>
		</select>
		<select name="sort" method="get">
		  <option value="sort">sort</option>
		  <option value="rsort">rsort</option>
		</select>
		<select name="feature" method="get">
		  <option value"docid">docid</option>
		  <option value="0">0</option>
		  <option value="1">1</option>
		  <option value="2">2</option>
		  <option value="3">3</option>
		  <option value="4">4</option>
		  <option value="5">5</option>
		  <option value="6">6</option>
		  <option value="7">7</option>
		  <option value="8">8</option>
		  <option value="9">9</option>
		  <option value="10">10</option>
		  <option value="11">11</option>
		  <option value="12">12</option>
		  <option value="13">13</option>
		  <option value="14">14</option>
		  <option value="15">15</option>
		  <option value="16">16</option>
		  <option value="17">17</option>
		  <option value="18">18</option>
		  <option value="19">19</option>
		  <option value="20">20</option>
		  <option value="21">21</option>
		  <option value="22">22</option>
		  <option value="23">23</option>
		  <option value="24">24</option>
		  <option value="25">25</option>
		  <option value="26">26</option>
		  <option value="27">27</option>
		  <option value="28">28</option>
		  <option value="29">29</option>
		  <option value="30">30</option>
		  <option value="31">31</option>
		  <option value="32">32</option>
		  <option value="33">33</option>
		</select>
  
		<input type="submit" name="subbtn" value="Submit">
		<br>
	</form>
	<form action= "/cgi-bin/index_search.sh" method = "get">
	  <b>Precalculated Queries:</b><br>
	  <select name="pre_dataset" method="get">
		  <option value="aquaint">aquaint</option>
		  <option value="trec08">trec08</option>
		  <option value="trec45">trec45</option>
	  </select>
	<?php
	   $trec08 = file_get_contents('/silo/datasets/ranking_data/queries.08enterprise.CE051-127.txt',true);
	   $trec45 = file_get_contents('/silo/datasets/ranking_data/queries.04robust.301-450_601-700.txt',true);
	   $aquaint = file_get_contents('/silo/datasets/ranking_data/queries.aquaint.303-589_50.txt',true);
	   $trec08_queries = explode("\n",$trec08);
	   $trec45_queries = explode("\n",$trec45);
	   $aquaint_queries = explode("\n",$aquaint);
	   echo '<select name="aquaint_query" method="get">' . "\n";
	   foreach($aquaint_queries as $value) {
	        echo ' <option value="' . $value . '">' . $value . "</option>\n";
	   }
echo '</select>';
echo "\n";
	   echo '<select name="trec08_query" method="get">';
	   echo "\n";
	   foreach ($trec08_queries as $value) {
	     echo ' <option value="' . $value . '">' . $value .  "</option>\n";
	   }
echo '</select>';
echo "\n";
	   echo '<select name="trec45_query" method="get">' . "\n";
	   foreach($trec45_queries as $value) {
	        echo ' <option value="' . $value . '">' . $value . "</option>\n";
	   }
echo '</select>';
	?>
	   <input type="submit" name="predet" value="Submit">


</body>
</html>
