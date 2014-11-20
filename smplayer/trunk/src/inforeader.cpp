/*  smplayer, GUI front-end for mplayer.
    Copyright (C) 2006-2014 Ricardo Villalba <rvm@users.sourceforge.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "inforeader.h"
#include "global.h"
#include "preferences.h"
#include "inforeadermplayer.h"
#include "inforeadermpv.h"
#include "playerprocess.h"
#include "paths.h"
#include <QFileInfo>
#include <QDateTime>
#include <QSettings>
#include <QDebug>

using namespace Global;

InfoReader * InfoReader::static_obj = 0;

InfoReader * InfoReader::obj(const QString & mplayer_bin) {
	QString player_bin = mplayer_bin;
	if (player_bin.isEmpty()) {
		player_bin = pref->mplayer_bin;
	}
	if (!static_obj) {
		static_obj = new InfoReader(player_bin);
	} else {
		static_obj->setPlayerBin(player_bin);
	}
	return static_obj;
}

InfoReader::InfoReader(QString mplayer_bin, QObject * parent)
	: QObject(parent)
	, mplayer_svn(0)
	, is_mplayer2(false)
	, is_mpv(false)
{
	setPlayerBin(mplayer_bin);
}

InfoReader::~InfoReader() {
}

void InfoReader::setPlayerBin(const QString & bin) {
	mplayerbin = bin;

	QFileInfo fi(mplayerbin);
	if (fi.exists() && fi.isExecutable() && !fi.isDir()) {
		// mplayerbin = fi.absoluteFilePath();
	}
#ifdef Q_OS_LINUX
	else {
		QString fplayer = findApp(mplayerbin);
		qDebug() << "InfoReader::setPlayerBin: fplayer:" << fplayer;
		if (!fplayer.isEmpty()) mplayerbin = fplayer;
	}
#endif
	qDebug() << "InfoReader::setPlayerBin: mplayerbin:" << mplayerbin;
}

void InfoReader::getInfo() {
	QString inifile = Paths::configPath() + "/player_info.ini";
	QSettings set(inifile, QSettings::IniFormat);

	QString sname = mplayerbin;
	sname = sname.replace("/", "_").replace("\\", "_").replace(".", "_").replace(":", "_");
	QFileInfo fi(mplayerbin);
	if (fi.exists()) {
		sname += "_" + QString::number(fi.size());

		qDebug() << "InfoReader::getInfo: sname:" << sname;

		// Check if we already have info about the player in the ini file
		bool got_info = false;
		set.beginGroup(sname);
		if (set.value("size", -1).toInt() == fi.size()) {
			got_info = true;
			vo_list = convertListToInfoList(set.value("vo_list").toStringList());
			ao_list = convertListToInfoList(set.value("ao_list").toStringList());
			demuxer_list = convertListToInfoList(set.value("demuxer_list").toStringList());
			vc_list = convertListToInfoList(set.value("vc_list").toStringList());
			ac_list = convertListToInfoList(set.value("ac_list").toStringList());
			mplayer_svn = set.value("mplayer_svn").toInt();
			mpv_version = set.value("mpv_version").toString();
			mplayer2_version = set.value("mplayer2_version").toString();
			is_mplayer2 = set.value("is_mplayer2").toBool();
			is_mpv = set.value("is_mpv").toBool();
		}
		set.endGroup();
		if (got_info) {
			qDebug() << "InfoReader::getInfo: loaded info from" << inifile;
			return;
		}
	}

	if (PlayerID::player(mplayerbin) == PlayerID::MPV) {
		qDebug("InfoReader::getInfo: mpv");
		InfoReaderMPV ir(mplayerbin, this);
		ir.getInfo();
		vo_list = ir.voList();
		ao_list = ir.aoList();
		demuxer_list = ir.demuxerList();
		vc_list = ir.vcList();
		ac_list = ir.acList();
		mplayer_svn = ir.mplayerSVN();
		mpv_version = ir.mpvVersion();
		mplayer2_version = "";
		is_mplayer2 = false;
		is_mpv = true;
	} else {
		qDebug("InfoReader::getInfo: mplayer");
		InfoReaderMplayer ir(mplayerbin, this);
		ir.getInfo();
		vo_list = ir.voList();
		ao_list = ir.aoList();
		demuxer_list = ir.demuxerList();
		vc_list = ir.vcList();
		ac_list = ir.acList();
		mplayer_svn = ir.mplayerSVN();
		mpv_version = "";
		mplayer2_version = ir.mplayer2Version();
		is_mplayer2 = ir.isMplayer2();
		is_mpv = false;
	}

	if (fi.exists()) {
		qDebug() << "InfoReader::getInfo: saving info to" << inifile;
		set.beginGroup(sname);
		set.setValue("size", fi.size());
		set.setValue("date", fi.lastModified());
		set.setValue("vo_list", convertInfoListToList(vo_list));
		set.setValue("ao_list", convertInfoListToList(ao_list));
		set.setValue("demuxer_list", convertInfoListToList(demuxer_list));
		set.setValue("vc_list", convertInfoListToList(vc_list));
		set.setValue("ac_list", convertInfoListToList(ac_list));
		set.setValue("mplayer_svn", mplayer_svn);
		set.setValue("mpv_version", mpv_version);
		set.setValue("mplayer2_version", mplayer2_version);
		set.setValue("is_mplayer2", is_mplayer2);
		set.setValue("is_mpv", is_mpv);
		set.endGroup();
	}
}

QString InfoReader::playerVersion() {
	QString player = QString("MPlayer SVN r%1").arg(mplayer_svn);

	if (is_mplayer2) {
		player = "MPlayer2 " + mplayer2_version;
	}
	else
	if (is_mpv) {
		player = "MPV " + mpv_version;
	}

	return player;
}

QStringList InfoReader::convertInfoListToList(InfoList l) {
	QStringList r;
	for (int n = 0; n < l.count(); n++) {
		r << l[n].name() + "|" + l[n].desc();
	}
	return r;
}

InfoList InfoReader::convertListToInfoList(QStringList l) {
	InfoList r;
	for (int n = 0; n < l.count(); n++) {
		QStringList s = l[n].split("|");
		if (s.count() >= 2) {
			r.append(InfoData(s[0], s[1]));
		}
	}
	return r;
}

#ifdef Q_OS_LINUX
QString InfoReader::findApp(const QString & appname) {
	QString res;

	QProcess p;
	p.start("which", QStringList() << appname);
	bool success = p.waitForFinished();
	if (success) {
		res = p.readAll();
		res = res.trimmed();
	}

	return res;
}
#endif

#include "moc_inforeader.cpp"