<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0"
                xmlns:xt="http://www.jclark.com/xt"
                extension-element-prefixes="xt">


<!-- leave empty (zero-length) to mean false -->
<xsl:variable name="generateServerSideCommands">t</xsl:variable>

<!-- the stylesheet to use with the generated html pages -->
<xsl:variable name="css-stylesheet">/skin/style.css</xsl:variable>


<xsl:template name="serverSideInclude">
	<xsl:param name="code"/>
	<xsl:if test="$generateServerSideCommands">
		<xsl:comment><xsl:value-of select="$code"/></xsl:comment>
	</xsl:if>
	<xsl:if test="not($generateServerSideCommands)">
		&lt;!-- <xsl:value-of select="$code"/> --&gt;
	</xsl:if>
</xsl:template>




<xsl:template match="/">
	<html>
		<xsl:apply-templates/>
	</html>
</xsl:template>

<xsl:template match="faq">
	<xsl:apply-templates select="head"/>
	<xsl:apply-templates select="body"/>
</xsl:template>

<xsl:template match="head">
	<head>
    	<title><xsl:value-of select="title"/> <xsl:value-of select="version" /></title>
	</head>
</xsl:template>

<xsl:template match="body">

<BODY>
	<H1>
	    <xsl:value-of select="/faq/head/title"/>
		<xsl:value-of select="version" />
	</H1>

	<xsl:apply-templates select="/faq/head/summary"/>

	<HR/>

	<H2>Table of Contents</H2>
		<ul>
			<xsl:apply-templates select="section" mode="TOC"/>
		</ul>

	<xsl:apply-templates select="about-section" mode="TOC"/>

	<xsl:apply-templates select="about-section"/>

	<!-- A download section -->
	<HR/>

	<H2>Download the FAQ</H2>
		<xsl:apply-templates select="/faq/head/download"/>

</BODY>

	<!-- contents of each section -->
	<xsl:apply-templates select="section"/>

</xsl:template>




<xsl:template match="summary">
	<H2>Overview</H2>
	<DIV>
	<xsl:apply-templates/>
	</DIV>
</xsl:template>





<xsl:template match="maintainers">
	<xsl:apply-templates/>
</xsl:template>




<xsl:template match="maintainer">
	<A href="mailto:{email}">
		<xsl:value-of select="name"/>
	</A>
</xsl:template>





<xsl:template match="section" mode="TOC">
	<li><a href="{@id}.shtml"><xsl:value-of select="title"/></a></li>
</xsl:template>





<xsl:template match="about-section" mode="TOC">
	<p><a href="{@id}.shtml"><xsl:value-of select="title"/></a></p>
</xsl:template>





<xsl:template match="section | about-section">
	<xt:document method="html" href="{@id}.shtml">

    <html>
	<head>
		<title>[NEdit] FAQ: <xsl:value-of select="title"/></title>
	</head>

	<body>

		<H1><xsl:value-of select="title"/></H1>

		<!-- include the paragraphs outside of qna-s -->
		
		<xsl:apply-templates select="p|div"/>
		
		<ol>
		<xsl:apply-templates select="qna" mode="TOC"/>
		</ol>

		
		<HR/>

		<xsl:apply-templates select="qna"/>
	
		<xsl:variable name="prev-sect"><xsl:value-of
			select="preceding-sibling::*[position() = 1]/@id"/></xsl:variable>
		<xsl:variable name="prev-title"><xsl:value-of
			select="id($prev-sect)/title"/></xsl:variable>
		<xsl:variable name="next-sect"><xsl:value-of
			select="following-sibling::*[position() = 1]/@id"/></xsl:variable>
		<xsl:variable name="next-title"><xsl:value-of
			select="id($next-sect)/title"/></xsl:variable>


  		<xsl:if test="$prev-sect != ''">
			[<A HREF="{$prev-sect}.shtml">
			<xsl:value-of select="$prev-title"/></A>]
		</xsl:if>

		[<A HREF="index.shtml">FAQ Contents</A>]

		<xsl:if test="$next-sect != ''">
			[<A HREF="{$next-sect}.shtml">
			<xsl:value-of select="$next-title"/></A>]
		</xsl:if>

	</body>

	</html>

	</xt:document>
</xsl:template>








<xsl:template match="qna" mode="TOC">
	<li>
	<A HREF="#{@id}">
		<xsl:apply-templates select="q" mode="TOC"/>
	</A>
	</li>
</xsl:template>


<xsl:template match="q" mode="TOC">
	<xsl:apply-templates/>
</xsl:template>


<xsl:template match="qna">
	<xsl:variable name="prefix"><xsl:number value="position()"/>. </xsl:variable>
	<A NAME="{@id}"></A>
	<xsl:if test="long-q">
		<xsl:apply-templates select="long-q/*[position()=1]">
			<xsl:with-param name="prefix"><xsl:value-of select="$prefix"/></xsl:with-param>
			<xsl:with-param name="citation">true</xsl:with-param>
		</xsl:apply-templates>
		
		<xsl:apply-templates select="long-q/*[position()>1]">
			<xsl:with-param name="citation">true</xsl:with-param>
		</xsl:apply-templates>
	</xsl:if>
	<xsl:if test="not(long-q)">
		<CITE><B>
		<xsl:value-of select="$prefix"/><xsl:apply-templates select="q"/>
		</B></CITE>
	</xsl:if>
	
	<xsl:apply-templates select="a"/>
	
	<HR/>
</xsl:template>

<xsl:template match="q">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="a">
	<xsl:apply-templates/>
</xsl:template>




<!-- paragraph and divisions -->

<xsl:template match="ul|ol|blockquote|p|pre">
	<xsl:param name="prefix"/>
	<xsl:param name="citation"/>
	<xsl:choose>
		<xsl:when test="$citation">
			<cite id="FAQ"><b>
			<xsl:element name="{name()}">
				<xsl:attribute name="class">FAQ</xsl:attribute>
				<xsl:value-of select="$prefix"/>
				<xsl:apply-templates/>
			</xsl:element>
			</b></cite>
		</xsl:when>
		<xsl:otherwise>
			<xsl:element name="{name()}">
				<xsl:attribute name="class">FAQ</xsl:attribute>
				<xsl:value-of select="$prefix"/>
				<xsl:apply-templates/>
			</xsl:element>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template match="li">
	<li><xsl:apply-templates/></li>
</xsl:template>

<!-- style elements -->

<xsl:template match="email">
	<a href="mailto:{.}"><xsl:value-of select="."/></a>
</xsl:template>

<xsl:template match="site">
	<a href="{.}" id="{@id}" class="{@class}"><xsl:value-of select='.'/></a>
</xsl:template>

<xsl:template match="link">
	<xsl:variable name="text"><xsl:apply-templates/></xsl:variable>
	<xsl:variable name="text1">
		<xsl:if test="$text != ''">
			<xsl:value-of select="$text"/>
		</xsl:if>
		<xsl:if test="$text = ''">
			<xsl:value-of select="@href"/>
		</xsl:if>
	</xsl:variable>
	<a href="{@href}"><xsl:value-of select="$text1"/></a>
</xsl:template>

<xsl:template match="img">
	<img>
		<xsl:attribute name="src"><xsl:value-of select="src"/></xsl:attribute>
		<xsl:attribute name="alt"><xsl:value-of select="alt"/></xsl:attribute>
	</img>
</xsl:template>





<xsl:template match="em">
	<em><xsl:apply-templates/></em>
</xsl:template>

<xsl:template match="strong">
	<strong><xsl:apply-templates/></strong>
</xsl:template>

<xsl:template match="tt">
	<tt><xsl:apply-templates/></tt>
</xsl:template>

<xsl:template match="code">
	<code><xsl:apply-templates/></code>
</xsl:template>


</xsl:stylesheet>

<!-- $Id: faq.xsl,v 1.3 2002/09/26 12:37:37 ajhood Exp $ -->
