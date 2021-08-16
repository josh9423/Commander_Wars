#pragma once
#include "3rd_party/oxygine-framework/oxygine/oxygine-forwards.h"
#include "3rd_party/oxygine-framework/oxygine/res/CreateResourceContext.h"
#include "3rd_party/oxygine-framework/oxygine/res/ResAnim.h"
#include "3rd_party/oxygine-framework/oxygine/res/Resource.h"

namespace oxygine
{
    class ResAtlas;
    using spResAtlas = intrusive_ptr<ResAtlas>;

    class ResAtlas: public Resource
    {
    public:
        static spResource create(CreateResourceContext& context);
        struct atlas
        {
            spTexture base;
            QString base_path;
            spTexture alpha;
            QString alpha_path;
        };
        explicit ResAtlas() = default;
        virtual ~ResAtlas();
        void addAtlas(ImageData::TextureFormat tf, QString base, QString alpha, qint32 w, qint32 h);
        const atlas& getAtlas(qint32 i) const
        {
            return m_atlasses[i];
        }
        qint32 getNum() const
        {
            return m_atlasses.size();
        }
        virtual void setLinearFilter(quint32 linearFilter) override;
        virtual quint32 getLinearFilter() const override;

    protected:
        void _load(LoadResourcesContext*) override;
        void _unload() override;
        spResAnim createEmpty(const XmlWalker& walker, CreateResourceContext& context);
        static void init_resAnim(spResAnim rs, QString file, QDomElement node);
        void loadBase(QDomElement node);
    private:
        void load_texture(QString file, spTexture nt, quint32 linearFilter, bool clamp2edge, LoadResourcesContext* load_context);
        void load_texture_internal(QString file, spTexture nt, quint32 linearFilter, bool clamp2edge, LoadResourcesContext* load_context);

    protected:
        //settings from xml
        quint32 m_linearFilter{GL_LINEAR};
        bool m_clamp2edge{true};
        QVector<unsigned char> m_hitTestBuffer;
        using atlasses = QVector<atlas>;
        atlasses m_atlasses;
    };
}
