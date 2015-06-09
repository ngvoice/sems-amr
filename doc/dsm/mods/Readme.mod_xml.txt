mod_xml - generic XML functions

This module implements generic XML handling functions. XML objects are created in
objects, which must be tracked or released by the script using trackObject/freeObject.
Then functions can be executed on the XML objects, e.g. XPath evaluation.

mod_xml must be preloaded to initialize libxml2.

xml.parse("<xml_obj/>", dst_objname)
  parse XML string into object referenced with dst_objname.
  Example: xml.parse($myxml, xml_obj)

xml.parseSIPMsgBody(msg_body_object, dst_objname)
  Parse a SIP message body object into an XML object.
  Example: xml.parseSIPMsgBody("SipSubscriptionBody", "substatus")

xml.evalXPath(xpath_expr, xml_object)
  Evaluate XPath expression on XML object.
  Namespaces are registered in $xml_object.ns variable.
  The result is saved to xml_object.xpath object.
  Example: 
    set($substatus.ns="a=urn:ietf:params:xml:ns:reginfo")
    xml.evalXPath("//a:contact[@state='active']", "substatus");

xml.XPathResultCount($cntvar=xpath_object)
  Save result count of XPath result in xpath_object into counter variable $cntvar.
  Example:
    xml.XPathResultCount($rescnt="substatus.xpath");

xml.getXPathResult($dstvar=xpath_object)
  save contents of xpath result object into $dstvar,
  or $dstvar[0] .. $dstvar[n] if more than one result 

xml.printXPathResult($dstvar=xpath_object)
  print information abut XPath result into $dstvar (with object names)

xml.updateXPathResult(xpath_object=content) - update xpath object with content (xml string)
xml.updateXPathResult(xpath_object[index]=content) - update only object at position index

xml.docDump($varname=xml_object) - dump xml_object to variable $varname

xml.setLoglevel(level)
 set libxml2 error logging level. Default: error
 Valid: error, warn, info, debug
