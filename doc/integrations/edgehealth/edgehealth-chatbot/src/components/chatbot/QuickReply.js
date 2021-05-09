import React from 'react';


const QuickReply = (props) => {
    if (props.reply.payload) {
        return (
            <a style={{ margin: 3}} href="/" className="btn-floating btn-large waves-effect waves-light red"
               onClick={(event) =>
                   props.click(
                       event,
                       props.reply.payload,
                       props.reply.text
                   )
               }>
                {props.reply.text}
            </a>
        );
    } else {
        return (
            <a style={{ margin: 3}} href={props.reply.link}
               className="btn-floating btn-large waves-effect waves-light red">
                {props.reply.text}
            </a>
        );
    }

};

export default QuickReply;
