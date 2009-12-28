.SUFFIXES: .po .pox .mo

POFILES=ru_RU.po nl_NL.po sv_SE.po hu_HU.po es_ES.po pt_BR.po fr_FR.po \
da_DK.po de_DE.po no_NO.po pl_PL.po ro_RO.po id_ID.po eo.po hr_HR.po pt_PT.po \
zh_TW.po

.pox.po: $*.pox pennmush.pot
	-msgmerge -E -v $*.pox pennmush.pot > $*.po

po-files: $(POFILES)

localized: $(LANG).po
	-msgfmt --strict $(LANG).po
	-mkdir -p $(LANG)/LC_MESSAGES
	-mv messages.mo $(LANG)/LC_MESSAGES/pennmush.mo

.po:
	@echo "No LANG variable set, no localization. Ignore errors."

clean:
	-rm $(POFILES)
