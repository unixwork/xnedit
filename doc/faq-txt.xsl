<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0"
                xmlns:xt="http://www.jclark.com/xt"
                extension-element-prefixes="xt">

<xsl:output method="html" indent="yes" />
<xsl:output doctype-system="faq-txt.dtd" />

<xsl:template name="hier"
	><xsl:if test="local-name(.) != ''"
		><xsl:for-each select=".."
			><xsl:call-template name="hier"
		/>/</xsl:for-each
	></xsl:if
	><xsl:value-of select="local-name()"
/></xsl:template>



<xsl:template match="/">
	<html><body>
		<xsl:apply-templates />
	</body></html>
</xsl:template>


<!-- head block -->

<xsl:template match="head">
	<xsl:apply-templates select="title"/>
	<xsl:apply-templates select="version"/>
	<xsl:apply-templates select="summary"/>
	<xsl:apply-templates select="maintainers"/>
</xsl:template>

<xsl:template match="version">
	<P>Version <xsl:value-of select="."/></P>
</xsl:template>


<xsl:template match="maintainers">
	<P>FAQ Maintainers: <xsl:apply-templates/></P>
</xsl:template>



<xsl:template match="maintainer">
	<xsl:value-of select="name"/> (<xsl:value-of select="email"/>)
</xsl:template>

<xsl:template match="download">
	<!-- don't display this section -->
</xsl:template>

<!-- qna-s -->


<xsl:template match="title">
<H1>
	<xsl:attribute name="node">
		<xsl:call-template name="hier"/>
	</xsl:attribute>
	<xsl:apply-templates/>
</H1>
</xsl:template>


<xsl:template match="qna">
	<xsl:choose>
		<xsl:when test="long-q">
			<I>
			<xsl:apply-templates select="long-q"/>
			</I>
		</xsl:when>
		<xsl:otherwise>
			<I>
			<xsl:apply-templates select="q"/>
			</I>
		</xsl:otherwise>
	</xsl:choose>
	<xsl:apply-templates select="a"/>
</xsl:template>



<xsl:template match="q">
<P>
	<xsl:attribute name="node">
		<xsl:call-template name="hier"/>
	</xsl:attribute>
	<xsl:attribute name="id"><xsl:value-of select="generate-id()"/></xsl:attribute>
	<xsl:apply-templates/>
</P>
</xsl:template>




<xsl:template match="p">
	
	<xsl:attribute name="node">
		<xsl:call-template name="hier"/>
	</xsl:attribute>
	<xsl:attribute name="id"><xsl:value-of select="generate-id()"/></xsl:attribute>
	
	<P><xsl:apply-templates/></P>
</xsl:template>

<xsl:template match="pre">
	
	<xsl:attribute name="node">
		<xsl:call-template name="hier"/>
	</xsl:attribute>
	<xsl:attribute name="id"><xsl:value-of select="generate-id()"/></xsl:attribute>
	
	<PRE><xsl:apply-templates/></PRE>
</xsl:template>

<xsl:template match="ul|ol">
	
	<xsl:attribute name="node">
		<xsl:call-template name="hier"/>
	</xsl:attribute>
	<xsl:attribute name="id"><xsl:value-of select="generate-id()"/></xsl:attribute>
	
	<BLOCKQUOTE><xsl:apply-templates/></BLOCKQUOTE>
</xsl:template>

<xsl:template match="li">
	
	<xsl:attribute name="node">
		<xsl:call-template name="hier"/>
	</xsl:attribute>
	<xsl:attribute name="id"><xsl:value-of select="generate-id()"/></xsl:attribute>
	
	<UL>* <xsl:apply-templates/></UL>
</xsl:template>



<xsl:template match="blockquote">
	
	<xsl:attribute name="node">
		<xsl:call-template name="hier"/>
	</xsl:attribute>
	<xsl:attribute name="id"><xsl:value-of select="generate-id()"/></xsl:attribute>
	
	<BLOCKQUOTE><xsl:apply-templates/></BLOCKQUOTE>
</xsl:template>
	





<!-- style and link elements -->

<xsl:template match="img">
	[<xsl:value-of select="alt"/>]
</xsl:template>

<xsl:template match="link">
	<xsl:apply-templates/>
	<xsl:text> (</xsl:text>
	<xsl:value-of select="@href"/>
	<xsl:text>)</xsl:text>
</xsl:template>

<xsl:template match="code">
	<xsl:text>'</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>'</xsl:text>
</xsl:template>

<xsl:template match="em">
	<xsl:text>'</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>'</xsl:text>
</xsl:template>



</xsl:stylesheet>

<!-- $Id: faq-txt.xsl,v 1.3 2002/09/26 12:37:37 ajhood Exp $ -->
