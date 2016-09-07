
<!DOCTYPE html>
<html>
<title>Secure Search Front End Tool</title>
<body>
<h1 style="font-size:300%;text-align:center;">Secure Search</h1>
 

   <form action= "searched.php" method = "get">
   <div align="center">
   <div style="position:relative;width:500px;height:60px;border:0;padding:0;margin:0;">


<!--creates pre-calculated select bar-->
   <select name="preCalcDropdown" style="position:absolute;top:0px;left:0px;width:390px; height:25px;line-height:20px;margin:0;padding:0;" onchange="document.getElementById('displayValue').value=this.options[this.selectedIndex].text; document.getElementById('idValue').value=this.options[this.selectedIndex].value;">

   <?php
   $trec08 = file_get_contents('/silo/datasets/ranking_data/queries.08enterprise.CE051-127.txt',true);
$trec45 = file_get_contents('/silo/datasets/ranking_data/queries.04robust.301-450_601-700.txt',true);
$aquaint = file_get_contents('/silo/datasets/ranking_data/queries.aquaint.303-589_50.txt',true);
$trec08_queries = explode("\n",$trec08);
$trec45_queries = explode("\n",$trec45);
$aquaint_queries = explode("\n",$aquaint);

foreach($aquaint_queries as $value) {
  if($value != "")
    echo ' <option value="' . $value . '">' . $value . "</option>\n";
}
foreach ($trec08_queries as $value) {
  if($value != "")
    echo ' <option value="' . $value . '">' . $value .  "</option>\n";
}
foreach($trec45_queries as $value) {
  if($value != "")
    echo ' <option value="' . $value . '">' . $value . "</option>\n";
}
echo '<option selected value="default"></option>';
echo '</select>'
?>
<!--add the bar to type in queries-->
<input type="text" name="displayValue" placeholder="Enter your query" id="displayValue" style="position:absolute;top:0px;left:0px;width:368px;width:180px\9;#width:180px;height:23px; height:21px\9;#height:18px;border:1px solid #556;" onfocus="this.select()">


  <!--add the dataset select bar-->
  <select style="position:absolute;top:0px;right:0px;width:100px;height:25px;border:0;padding:0;margin:0;" name="dataset" method="get"> 
  <option value="aquaint">aquaint</option>
  <option value="trec08">trec08</option>
  <option value="trec45">trec45</option>
  </select>


 <!-- add submit button-->
<input type="submit" name="Submit" value="Submit" style="position:absolute;top:35px;left:225px;width:80px;height:25px;line-height:20px;margin:0;padding:0;">'

  </div>
  </form>

  </body>
  </html>
