var xmlHttp;

function stateCallback() {
  var stat, rstate;
  if( !xmlHttp ) return;

  try {
    rstate = xmlHttp.readyState;
  } catch (err) {
    alert(err);
  }

  switch( rstate )
  {
    // uninitialized
    case 0:
    // loading
    case 1:
    // loaded
    case 2:
    // interactive
    case 3:
    break;
    // complete, so act on response
    case 4:
    // check http status
      try {
        stat = xmlHttp.status;
      }
      catch (err) {
        stat = "xmlHttp.status does not exist";
      }
      if( stat == 200 )    // success
      {
          AJAX_response(xmlHttp.responseText);
      }
      // loading not successfull, e.g. page not available
      else { }
  }
}

function init_AJAX() 
{
  var new_xmlHttp;

  try
  {
    // Internet Explorer
    if( window.ActiveXObject )
    {
      for( var i = 5; i; i-- )
      {
        try
        {
          // loading of a newer version of msxml dll (msxml3 - msxml5) failed
          // use fallback solution
          // old style msxml version independent, deprecated
          if( i == 2 ) {
            new_xmlHttp = new ActiveXObject("Microsoft.XMLHTTP");
          }
          // try to use the latest msxml dll
          else {
            new_xmlHttp = new ActiveXObject( "Msxml2.XMLHTTP." + i + ".0" );
          }
          break;
        }
        catch( excNotLoadable ) {
          new_xmlHttp = false;
        }
      }
    }
    // Mozilla, Opera und Safari
    else if( window.XMLHttpRequest ) {
      new_xmlHttp = new XMLHttpRequest();
    }
  }
  catch( excNotLoadable ) {
    new_xmlHttp = false;
  }

  new_xmlHttp.onreadystatechange = stateCallback;

  xmlHttp = new_xmlHttp;
}

function AJAX_get(url) {
  if( xmlHttp ) {
    xmlHttp.abort();
    xmlHttp = false;
  }

  init_AJAX();
  xmlHttp.open("GET", url, true);
  xmlHttp.send(null);
}
