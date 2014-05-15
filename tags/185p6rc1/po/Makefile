SHORTLANG=$(basename $(LANG))

.SUFFIXES: .po .pox .mo

POFILES=ru_RU.po nl_NL.po sv_SE.po hu_HU.po es_ES.po pt_BR.po fr_FR.po \
da_DK.po de_DE.po no_NO.po pl_PL.po ro_RO.po id_ID.po eo.po hr_HR.po pt_PT.po \
zh_TW.po fi_FI.po

.pox.po: $*.pox pennmush.pot
	-msgmerge -E -v $*.pox pennmush.pot > $*.po

po-files: $(POFILES)

localized: $(SHORTLANG).po
	-msgfmt --strict $(SHORTLANG).po
	-mkdir -p $(SHORTLANG)/LC_MESSAGES
	-mv messages.mo $(SHORTLANG)/LC_MESSAGES/pennmush.mo

.po:
	@echo "No LANG variable set, no localization. Ignore errors."

clean:
	-rm $(POFILES)
