<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0"
                xmlns:xt="http://www.jclark.com/xt"
                extension-element-prefixes="xt">

<xsl:output method="text"/>






<xsl:template match="I">
	<xsl:param name="indent"/>
	<xsl:text>
PREFIXED</xsl:text>
	<xsl:apply-templates>
		<xsl:with-param name="indent"><xsl:value-of select="$indent"/></xsl:with-param>
	</xsl:apply-templates>
	<xsl:text>NOT_PREFIXED
</xsl:text>
</xsl:template>


<xsl:template match="BLOCKQUOTE">
	<xsl:param name="indent"/>
	<xsl:apply-templates select="*">
		<xsl:with-param name="indent"><xsl:value-of select="concat($indent, '    ')"/></xsl:with-param>
	</xsl:apply-templates>
</xsl:template>



<xsl:template match="P">
	<xsl:param name="indent"/>
<xsl:text>
</xsl:text>
	<xsl:apply-templates>
		<xsl:with-param name="indent"><xsl:value-of select="$indent"/></xsl:with-param>
	</xsl:apply-templates>
<xsl:text>
</xsl:text>
</xsl:template>



<xsl:template match="UL">
	<xsl:param name="indent"/>
<xsl:text>
</xsl:text>
	<xsl:apply-templates>
		<xsl:with-param name="indent"><xsl:value-of select="$indent"/></xsl:with-param>
	</xsl:apply-templates>
<xsl:text>
</xsl:text>
</xsl:template>




<xsl:template match="H1">
<xsl:text>


</xsl:text>
	<xsl:variable name="val"><xsl:apply-templates/></xsl:variable>
	<xsl:variable name="len"><xsl:value-of select="string-length($val)"/></xsl:variable>
	<xsl:variable name="underscore"><xsl:value-of
		select="substring('----------------------------------------------------------------------', 1, $len)"/></xsl:variable>
	<xsl:value-of select="translate($val, 'abcdefghijklmnopqrstuvwxyz', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
<xsl:text>
</xsl:text>
	<xsl:value-of select="$underscore"/>
<xsl:text>

</xsl:text>
</xsl:template>



<xsl:template match="PRE">
	<xsl:text>
PRE</xsl:text>
	<xsl:for-each select="text()">
		<xsl:value-of select="."/>
	</xsl:for-each>
	<xsl:text>NOT_PRE
</xsl:text>
</xsl:template>





<xsl:template match="text()">
	<xsl:param name="indent"/>
	<xsl:value-of select="$indent"/>
	<xsl:value-of select="normalize-space()" />
</xsl:template>

</xsl:stylesheet>

<!-- $Id: faq-txt-pass2.xsl,v 1.3 2002/09/26 12:37:37 ajhood Exp $ -->
