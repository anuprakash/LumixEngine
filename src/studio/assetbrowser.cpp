#include "assetbrowser.h"
#include "ui_assetbrowser.h"
#include "file_system_watcher.h"
#include "core/crc32.h"
#include "core/log.h"
#include "core/resource.h"
#include "core/resource_manager.h"
#include "editor/world_editor.h"
#include "engine/engine.h"
#include "insert_mesh_command.h"
#include "notifications.h"
#include "scripts/scriptcompiler.h"
#include <qdesktopservices.h>
#include <qfilesystemmodel.h>
#include <qinputdialog.h>
#include <qlistwidget.h>
#include <qmenu.h>
#include <qmessagebox.h>
#include <qprocess.h>
#include <qurl.h>


struct ProcessInfo
{
	class QProcess* m_process;
	QString m_path;
	int m_notification_id;
};


void getDefaultFilters(QStringList& filters)
{
	filters << "*.msh" << "*.unv" << "*.ani" << "*.blend" << "*.tga" << "*.mat" << "*.dds" << "*.fbx";
}


AssetBrowser::AssetBrowser(QWidget* parent) :
	QDockWidget(parent),
	m_ui(new Ui::AssetBrowser)
{
	m_watcher = FileSystemWatcher::create(Lumix::Path(QDir::currentPath().toLatin1().data()));
	m_watcher->getCallback().bind<AssetBrowser, &AssetBrowser::onFileSystemWatcherCallback>(this);
	m_base_path = QDir::currentPath();
	m_editor = NULL;
	m_ui->setupUi(this);
	m_model = new QFileSystemModel;
	m_model->setRootPath(QDir::currentPath());
	QStringList filters;
	getDefaultFilters(filters);
	m_model->setReadOnly(false);
	m_model->setNameFilters(filters);
	m_model->setNameFilterDisables(false);
	m_ui->treeView->setModel(m_model);
	m_ui->treeView->setRootIndex(m_model->index(QDir::currentPath()));
	m_ui->treeView->hideColumn(1);
	m_ui->treeView->hideColumn(2);
	m_ui->treeView->hideColumn(3);
	m_ui->treeView->hideColumn(4);
	m_ui->listWidget->hide();
	connect(this, SIGNAL(fileChanged(const QString&)), this, SLOT(onFileChanged(const QString&)));
	connect(m_ui->treeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &AssetBrowser::onTreeViewSelectionChanged);
}

AssetBrowser::~AssetBrowser()
{
	delete m_ui;
	delete m_model;
}


void AssetBrowser::setWorldEditor(Lumix::WorldEditor& editor)
{
	m_editor = &editor;
	m_editor->registerEditorCommandCreator("insert_mesh", &AssetBrowser::createInsertMeshCommand);
}


Lumix::IEditorCommand* AssetBrowser::createInsertMeshCommand(Lumix::WorldEditor& editor)
{
	return editor.getAllocator().newObject<InsertMeshCommand>(editor);
}


void AssetBrowser::onTreeViewSelectionChanged(const QModelIndex&, const QModelIndex&)
{
    /*if (current.isValid())
	{
		const QFileInfo& file_info = m_model->fileInfo(current);
		QByteArray byte_array = file_info.filePath().toLower().toLatin1();
		const char* filename = byte_array.data();
		emit fileSelected(filename);
    }*/
}


void AssetBrowser::onFileSystemWatcherCallback(const char* path)
{
	emitFileChanged(path);
}


void AssetBrowser::emitFileChanged(const char* path)
{
	emit fileChanged(path);
}


void AssetBrowser::handleDoubleClick(const QFileInfo& file_info)
{
	const QString& suffix = file_info.suffix();
	QString file = file_info.filePath().toLower();
	if(suffix == "unv")
	{
		m_editor->loadUniverse(Lumix::Path(file.toLatin1().data()));
	}
	else if(suffix == "msh")
	{
		InsertMeshCommand* command = m_editor->getAllocator().newObject<InsertMeshCommand>(*m_editor, m_editor->getCameraRaycastHit(), Lumix::Path(file.toLatin1().data()));
		m_editor->executeCommand(command);
	}
	else if(suffix == "ani")
	{
		m_editor->addComponent(crc32("animable"));
		m_editor->setProperty(crc32("animable"), -1, *m_editor->getProperty("animable", "preview"), file.toLatin1().data(), file.length());
	}
	else if (suffix == "blend" || suffix == "tga" || suffix == "dds")
	{
		QDesktopServices::openUrl(QUrl::fromLocalFile(file_info.absoluteFilePath()));
	}
}


void AssetBrowser::on_treeView_doubleClicked(const QModelIndex &index)
{
	ASSERT(m_model);
	handleDoubleClick(m_model->fileInfo(index));
}


void AssetBrowser::onFileChanged(const QString& path)
{
	QFileInfo info(path);
	if (info.suffix() == "cpp")
	{
		m_compiler->onScriptChanged(info.fileName().toLatin1().data());
	}
	else if(info.suffix() == "blend@")
	{
		QFileInfo file_info(path);
		QString base_name = file_info.absolutePath() + "/" + file_info.baseName() + ".blend";
		QFileInfo result_file_info(base_name);
		exportAnimation(result_file_info);
		exportModel(result_file_info);
	}
	else if(m_editor)
	{
		m_editor->getEngine().getResourceManager().reload(path.toLatin1().data());
	}
}


void fillList(QListWidget& widget, const QDir& dir, const QStringList& filters)
{
	QFileInfoList list = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot, QDir::NoSort);

	for(int i = 0, c = list.size(); i < c; ++i)
	{
		QString filename = list[i].fileName();
		QListWidgetItem* item = new QListWidgetItem(list[i].fileName());
		widget.addItem(item);
		item->setData(Qt::UserRole, list[i].filePath());
	}
	
	list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);

	for(int i = 0, c = list.size(); i < c; ++i)
	{
		QString filename = list[i].fileName();
		fillList(widget, QDir(list[i].filePath()), filters);
	}
}


void AssetBrowser::on_searchInput_textEdited(const QString &arg1)
{
	if(arg1.length() == 0)
	{
		m_ui->listWidget->hide();
		m_ui->treeView->show();
	}
	else
	{
		QStringList filters;
		filters << QString("*") + arg1 + "*";
		m_ui->listWidget->show();
		m_ui->treeView->hide();
		QDir dir(QDir::currentPath());
		m_ui->listWidget->clear();
		fillList(*m_ui->listWidget, dir, filters);
	}
}


void AssetBrowser::on_listWidget_activated(const QModelIndex &index)
{
	QVariant user_data = m_ui->listWidget->item(index.row())->data(Qt::UserRole);
	QFileInfo info(user_data.toString());
	handleDoubleClick(info);
}


void AssetBrowser::on_exportFinished(int exit_code)
{
	QProcess* process = static_cast<QProcess*>(QObject::sender());
	QString s = process->readAll();
	process->deleteLater();
	while(process->waitForReadyRead())
	{
		s += process->readAll();
	}
	if (exit_code != 0)
	{
		auto msg = s.toLatin1();
		Lumix::g_log_error.log("editor") << msg.data();
	}

	for (auto iter = m_processes.begin(); iter != m_processes.end(); ++iter)
	{
		if (iter->m_process == process)
		{
			m_notifications->setNotificationTime(iter->m_notification_id, 1.0f);
			m_notifications->setProgress(iter->m_notification_id, 100);
			process->deleteLater();
			m_processes.erase(iter);
			break;
		}
	}
}


void AssetBrowser::exportAnimation(const QFileInfo& file_info)
{
	ProcessInfo process;
	process.m_path = file_info.path().toLatin1().data();
	process.m_process = new QProcess();
	auto message = QString("Exporting animation %1").arg(file_info.fileName()).toLatin1();
	process.m_notification_id = m_notifications->showProgressNotification(message.data());
	
	m_notifications->setProgress(process.m_notification_id, 50);
	m_processes.append(process);
	QStringList list;
	list.push_back("/C");
	list.push_back("models\\export_anim.bat");
	list.push_back(file_info.absoluteFilePath().toLatin1().data());
	list.push_back(m_base_path.toLatin1().data());
	connect(process.m_process, (void (QProcess::*)(int))&QProcess::finished, this, &AssetBrowser::on_exportFinished);
	process.m_process->start("cmd.exe", list);
}


void AssetBrowser::exportModel(const QFileInfo& file_info)
{
	ProcessInfo process;
	process.m_path = file_info.path().toLatin1().data();
	process.m_process = new QProcess();
	auto message = QString("Exporting model %1").arg(file_info.fileName()).toLatin1();
	process.m_notification_id = m_notifications->showProgressNotification(message.data());
	m_notifications->setProgress(process.m_notification_id, 50);
	m_processes.append(process);
	QStringList list;
	if (file_info.suffix() == "fbx")
	{
		list.push_back(file_info.absoluteFilePath());
		list.push_back(file_info.absolutePath() + "/" + file_info.baseName() + ".msh");
		connect(process.m_process, (void (QProcess::*)(int))&QProcess::finished, this, &AssetBrowser::on_exportFinished);
		process.m_process->start("editor/tools/fbx_converter.exe", list);
	}
	else
	{
		list.push_back("/C");
		list.push_back("models\\export_mesh.bat");
		list.push_back(file_info.absoluteFilePath().toLatin1().data());
		list.push_back(m_base_path.toLatin1().data());
		connect(process.m_process, (void (QProcess::*)(int))&QProcess::finished, this, &AssetBrowser::on_exportFinished);
		process.m_process->start("cmd.exe", list);
	}
}

void AssetBrowser::on_treeView_customContextMenuRequested(const QPoint &pos)
{
	QMenu *menu = new QMenu("Item actions",NULL);
	const QModelIndex& index = m_ui->treeView->indexAt(pos);
	const QFileInfo& file_info = m_model->fileInfo(index);
	QAction* selected_action = NULL;
	QAction* delete_file_action = new QAction("Delete", menu);
	menu->addAction(delete_file_action);
	QAction* rename_file_action = new QAction("Rename", menu);
	menu->addAction(rename_file_action);

	QAction* create_dir_action = new QAction("Create directory", menu);
	QAction* export_anim_action = new QAction("Export Animation", menu);
	QAction* export_model_action = new QAction("Export Model", menu);
	if (file_info.isDir())
	{
		menu->addAction(create_dir_action);
	}
	if (file_info.suffix() == "blend" || file_info.suffix() == "fbx")
	{
		menu->addAction(export_anim_action);
		menu->addAction(export_model_action);
	}
	selected_action = menu->exec(mapToGlobal(pos));
	if (selected_action == export_anim_action)
	{
		exportAnimation(file_info);
	}
	else if (selected_action == export_model_action)
	{
		exportModel(file_info);
	}
	else if (selected_action == delete_file_action)
	{
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, "Delete", "Are you sure?", QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes)
		{
			if (file_info.isFile())
			{
				QFile::remove(file_info.absoluteFilePath());
			}
			else
			{
				QDir dir(file_info.absoluteFilePath());
				dir.removeRecursively();
			}
		}
	}
	else if (selected_action == rename_file_action)
	{
		m_ui->treeView->edit(index);
	}
	else if (selected_action == create_dir_action)
	{
		bool ok;
		QString text = QInputDialog::getText(this, "Create directory", "Directory name:", QLineEdit::Normal, QDir::home().dirName(), &ok);
		if (ok && !text.isEmpty())
		{
			QDir().mkdir(file_info.absoluteFilePath() + "/" + text);
		}
	}
}

void AssetBrowser::on_filterComboBox_currentTextChanged(const QString&)
{
	QStringList filters;
	if(m_ui->filterComboBox->currentText() == "All")
	{
		getDefaultFilters(filters);
	}
	else if(m_ui->filterComboBox->currentText() == "Mesh")
	{
		filters << "*.msh";
	}
	else if(m_ui->filterComboBox->currentText() == "Material")
	{
		filters << "*.mat";
	}
	m_model->setNameFilters(filters);
}

void AssetBrowser::on_treeView_clicked(const QModelIndex &index)
{
	if (index.isValid())
	{
		const QFileInfo& file_info = m_model->fileInfo(index);
		if(file_info.isFile())
		{
			QByteArray byte_array = file_info.filePath().toLower().toLatin1();
			const char* filename = byte_array.data();
			emit fileSelected(filename);
		}
	}
}