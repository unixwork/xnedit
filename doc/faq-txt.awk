# $Id: faq-txt.awk,v 1.3 2002/09/26 12:37:37 ajhood Exp $

function wrap(line, prefix)
{

	wrapMargin = 75;

	if (length(line) < wrapMargin) {
		print prefix line;
	} else {

		count = split(line, words);

		indent = match(line, /[^ ]/)

		indentStr = substr( \
			"                                                              ", \
			1, indent - 1);

		result = "";
		for (w = 1; w <= count; w++) {
			if (length(result) + length(words[w]) > wrapMargin - indent) {
				# print a new line
				print prefix indentStr result;
				result = words[w];
			} else {
				if (w == 1)
					result = words[w]
				else
					result = result " " words[w];
			}
		}
		print prefix indentStr result;
	}
}




BEGIN { prefixed = 0; pre = 0 }
/^PREFIXED$/  {prefixed = 1; next }
/^NOT_PREFIXED$/  {prefixed = 0; next }
/^PRE$/  {pre = 1; next }
/^NOT_PRE$/  {pre = 0; next }
{
	if (pre) {
		if (prefixed) print "> " $0;
		else print $0;
	} else {
		if (prefixed) wrap($0, "> ");
		else wrap($0, "")
	}
}
